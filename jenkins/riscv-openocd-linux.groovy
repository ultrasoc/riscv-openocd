// pull in global functions
@Library("Core@master") _

// Build status format string see "MailResult" function
emailBodyFormat="<b>Build status : %s</b><br>\n<br>Project: %s<br>Build Number: %s<br> URL of build: %s"
// Subject of email
emailSubject="Jenkins CI: Project name -> ${env.JOB_NAME}"
// Only send 1 email per build
emailSent=0

testPath="test"
buildPath="linux-build"

// Gitlab token
def project_token = "openocd-riscv-branch-update"
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
        label "modern-linux"
    }

    options
    {
        disableConcurrentBuilds()
        timeout(time: 120, unit: "MINUTES")
        timestamps()
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
                fileOperations([folderDeleteOperation(buildPath)])
                fileOperations([folderCreateOperation(buildPath)])
                sh """

                # Build openocd on Linux.
                #
                git submodule update --init
                ./bootstrap
                cd ${buildPath}
                ../configure --disable-internal-libjaylink --enable-dummy --enable-remote-bitbang --disable-werror --enable-ust-jtagprobe --enable-ust-mmap
                make -j5
                make install prefix=${WORKSPACE}/usr
                cd ${WORKSPACE}
                tar -zcf openocd-linux-ustbin.tar.gz usr/bin
                ${WORKSPACE}/usr/bin/openocd -v 2> openocd-version.txt
                """
            }
        }
        stage("Archiving")
        {
            steps
            {
                echo "Archiving...."
                archive_artifacts( "gitlog" )
                archive_artifacts( "openocd-linux-ustbin.tar.gz" )
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
