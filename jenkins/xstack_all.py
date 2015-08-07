import sys
from os.path import dirname, realpath

sys.path.append(dirname(realpath(__file__)) + '/../ocr/jenkins')
#sys.path.append(dirname(realpath(__file__)) + '/../hll/cnc/jenkins')
sys.path.append(dirname(realpath(__file__)) + '/../apps/jenkins')
from ocr_all import *
#from cnc_ocr_all import *
from apps_all import *
