export MY_JJ_HOME=$HOME/jenkins-test-home
#./testrunner.py --debug --hier-results -r xstack:${MY_JJ_HOME}/init/xstack -p ${MY_JJ_HOME}/private -s ${MY_JJ_HOME}/shared -e ${MY_JJ_HOME}/env -o ${MY_JJ_HOME}/testrunner-results.xml -k cnc-ocr --no-clean all
./testrunner.py --hier-results -r xstack:${MY_JJ_HOME}/init/xstack -p ${MY_JJ_HOME}/private -s ${MY_JJ_HOME}/shared -e ${MY_JJ_HOME}/env -o ${MY_JJ_HOME}/testrunner-results.xml -k cnc-ocr --no-clean all
