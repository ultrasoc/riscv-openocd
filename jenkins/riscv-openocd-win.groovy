// pull in global functions
@Library("Core@master") _

// Build status format string see "MailResult" function
emailBodyFormat="<b>Build status : %s</b><br>\n<br>Project: %s<br>Build Number: %s<br> URL of build: %s"
// Subject of email
emailSubject="Jenkins CI: Project name -> ${env.JOB_NAME}"
// Only send 1 email per build
emailSent=0

testPath="test"
buildPath="build"

// Gitlab token
def project_token = "openocd-riscv-win-update"
properties([
    gitLabConnection("UltraSoC"),
    pipelineTriggers([
        [
            $class: "GitLabPushTrigger",
            secretToken: project_token
        ]
    ])
])

// Jenkins starts here
pipeline
{
    agent
    {
        label "win10-64bit-baremetal"
    }

    options
    {
        disableConcurrentBuilds()
        timeout(time: 120, unit: "MINUTES")
        timestamps()
    }

    environment
    {
        MSYSTEM = "MINGW64"
    }

    stages
    {
        stage("Checkout")
        {
            steps
            {
                // GitPrepare checks out code from git, recursing into submodules
                GitPrepare()
            }
        }
        stage("Build")
        {
            steps
            {
                RunUnixOnWin("uname -a")
                fileOperations([folderDeleteOperation(buildPath)])
                fileOperations([folderCreateOperation(buildPath)])
                RunUnixOnWin("./bootstrap")
                RunUnixOnWin("cd ${buildPath} ; ../configure --build=x86_64-pc-msys2 --host=x86_64-w64-mingw32 --disable-internal-libjaylink --enable-ust-jtagprobe --enable-ust-mmap --disable-werror --enable-static")
                RunUnixOnWin("cd ${buildPath} ; make -j5")
                // HACK to get open ocd to build statically. Needed as conf file is broken.
                RunUnixOnWin("cd ${buildPath} ; x86_64-w64-mingw32-gcc -Wall -Wstrict-prototypes -Wformat-security -Wshadow -Wextra -Wno-unused-parameter -Wbad-function-cast -Wcast-align -static -Wredundant-decls -g -O2 -o src/openocd.exe src/main.o  src/.libs/libopenocd.a -L/mingw64/lib -lusb-1.0 -lws2_32 ./jimtcl/libjim.a")
                RunUnixOnWin("echo %GIT_COMMIT% > openocd-version.txt")
                RunUnixOnWin("echo %JOB_NAME% >> openocd-version.txt")
                RunUnixOnWin("echo %BUILD_NUMBER% >> openocd-version.txt")
                RunUnixOnWin("cd ${buildPath} ; tar -zcf ../openocd-win64-ustbin.tar.gz src/openocd.exe")
            }
        }
        stage("Archiving")
        {
            steps
            {
                echo "Archiving...."
                archive_artifacts( "openocd-win64-ustbin.tar.gz" )
                archive_artifacts( "build/src/openocd.exe" )
                archive_artifacts( "openocd-version.txt" )
            }
        }
    }
    post
    {
        always
        {
            println JOB_NAME
            println BRANCH_NAME
        }
        failure
        {
            script
            {
                MailResult(currentBuild.result)
            }
        }
        unstable
        {
            script
            {
                MailResult(currentBuild.result)
            }
        }
        changed
        {
            script
            {
                MailResult(currentBuild.result)
            }
        }
    }
}


// Email build status
// ----------------------
// Mail report of build change
def MailResult(buildStatus)
{
    // Only send 1 email per run
    if( 0 == emailSent )
    {
        emailSent=1

        // Add all possible providers
        def emailProviders = [ [$class: "CulpritsRecipientProvider"], [$class: "DevelopersRecipientProvider"] ];
        emailProviders.add ( [$class: "RequesterRecipientProvider"] );

        // Status in console log
        echo "Sending email for build status ${buildStatus}"
        // Format up body of email
        emailBodyStr=sprintf(emailBodyFormat, [buildStatus, env.JOB_NAME, env.BUILD_NUMBER, env.BUILD_URL ]);
        // Send email
        emailext body: emailBodyStr, mimeType: "text/html", subject: emailSubject, recipientProviders: emailProviders;
    }
}
