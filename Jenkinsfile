pipeline {
    agent any
    stages {
        stage('Check style') {
            steps {
                sh 'resources/check-style.sh'
            }
        }
        stage('Build') {
            steps {
                sh '''export PATH=$PATH:/usr/local/bin
mkdir build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ..
ninja'''
            }
        }
        stage('Test') {
            steps {
                sh '''source ./setpath.sh
python3.4 -m pytest'''
            }
        }
    }
}
