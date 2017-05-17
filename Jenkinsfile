pipeline {
    agent any

    stages {
        stage('Check style') {
            steps {
                ansiColor('xterm') {
                    sh 'resources/check-style.sh'
                }
            }
        }
        stage('Clone dependencies') {
            steps {
                sh 'sed -i.bak "s/https:\\/\\/github.com\\/mitsuba-renderer\\/enoki/git@github.com:mitsuba-renderer\\/enoki.git/g" .gitmodules'
                sh 'git submodule update --init --recursive'
                sh 'mv .gitmodules.bak .gitmodules'
            }
        }
        stage('Build [release, single precision]') {
            steps {
                sh '''export PATH=$PATH:/usr/local/bin
rm -Rf build
mkdir -p build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DMTS_DOUBLE_PRECISION=OFF ..
ninja'''
            }
        }
        stage('Run tests [release, single precision]') {
            steps {
                sh '''source ./setpath.sh
python3.4 -m pytest'''
            }
        }
        stage('Build [debug, double precision]') {
            steps {
                sh '''export PATH=$PATH:/usr/local/bin
rm -Rf build
mkdir -p build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DMTS_DOUBLE_PRECISION=ON ..
ninja'''
            }
        }
        stage('Run tests [debug, double precision]') {
            steps {
                sh '''source ./setpath.sh
python3.4 -m pytest'''
            }
        }
    }

    post {
        always {
            deleteDir()
        }

        success {
            slackSend (color: '#00FF00', message: "SUCCESSFUL: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.RUN_DISPLAY_URL})")
        }

        failure {
            slackSend (color: '#FF0000', message: "FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.RUN_DISPLAY_URL})")

            emailext (
                    subject: "FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'",
                    body: """<p>FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]':</p>
                        <p>Check console output at &QUOT;<a href='${env.RUN_DISPLAY_URL}'>${env.JOB_NAME} [${env.BUILD_NUMBER}]</a>&QUOT;</p>""",
                    recipientProviders: [[$class: 'DevelopersRecipientProvider']]
            )
        }
    }
}
