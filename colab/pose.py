# -*- coding: utf-8 -*-
"""pose.ipynb

Automatically generated by Colaboratory.

Original file is located at
    https://colab.research.google.com/drive/186N1ZF5UrCCD2lKMWjQYAAK35DV9Geje
"""

!wget 'https://drive.google.com/uc?export=dowload&id=1T2Kq01WXzPMrQdnEOUEiVBhwouW8Pka5' -O pose.onnx
!wget 'https://github.com/opencv/opencv_extra/raw/master/testdata/dnn/pose.png' -O person.png
!wget 'https://github.com/opencv/opencv_extra/raw/master/testdata/dnn/keypoints_exp.npy'

!wget 'https://github.com/opencv/opencv_extra/raw/master/testdata/dnn/gray_face.png' -O face.png
!wget 'https://github.com/opencv/opencv_extra/raw/master/testdata/dnn/facial_keypoints_exp.npy'
!wget 'https://drive.google.com/uc?export=dowload&id=1etGXT9WQK1KjDkJ0pUTH-CaHHva4p9cY' -O facial_keypoints.onnx

# Commented out IPython magic to ensure Python compatibility.
# %cd /content
!ls -l
!md5sum pose.onnx
!sha1sum pose.onnx

from google.colab.patches import cv2_imshow
import cv2, numpy as np

def check(img,onnx,npy,siz,mn):
  net = cv2.dnn.readNet(onnx)
  #cv2_imshow(img)
  #blob = cv2.dnn.blobFromImage(img,1./255,(386,386),(128,128,128))
  blob = cv2.dnn.blobFromImage(img,1./255,siz, mn, False,False)
  net.setInput(blob)
  out = net.forward()
  net.dumpToFile("pose_dot.txt")
  exp = np.load(npy)
  
  S = np.shape(out)
  print(S, np.shape(exp))
  if len(S)==4:
    N = S[1]
    W = S[2]
    H = S[3]
    SX = img.shape[1] / W
    SY = img.shape[0] / H
    print(SX,SY)
    j = 0
    for i in range(0,N-1):
      heat = out[0,i,:,:]
      mv,Mv,mp,Mp = cv2.minMaxLoc(heat)
      cx = int(Mp[0] * SX)
      cy = int(Mp[1] * SY)
      cx1=-1;
      cy1=-1
      if Mv>0.5:      
        cv2.circle(img, (cx,cy), 3, (0,0,200), -1)
        cx1 = int(exp[j,0])
        cy1 = int(exp[j,1])
        j += 1
        cv2.circle(img, (cx1,cy1), 5, (0,200,0), 2)
      print(i,Mp,Mv, cx,cy, cx1,cy1)
  else:
    for i in range(0,exp.shape[0]):
      cx = int(out[0,i,0] * img.shape[1]) 
      cy = int(out[0,i,1] * img.shape[0])
      cv2.circle(img, (cx,cy), 3, (0,0,200), -1)
      cx = int(exp[i,0] * img.shape[1])
      cy = int(exp[i,1] * img.shape[0])
      cv2.circle(img, (cx,cy), 5, (0,200,0), 2)

  cv2_imshow(img)


check(cv2.imread("person.png"), "pose.onnx", "keypoints_exp.npy",(256,256),(128,128,128))
#check(cv2.imread("face.png",0), "facial_keypoints.onnx", "facial_keypoints_exp.npy",(224,224),())

!dot pose_dot.txt -Tpng -opose_dot.png
#!head -c 5000 pose_dot.png
im=cv2.imread("pose_dot.png")
print(np.shape(im))
cv2_imshow(im)

outs = ['stage_0_output_1_heatmaps', 'stage_0_output_0_pafs',
                    'stage_1_output_1_heatmaps', 'stage_1_output_0_pafs']

net = cv2.dnn.readNet("pose.onnx")
blob = cv2.dnn.blobFromImage(cv2.imread("person.png"), 1./255, (256,256), (128,128,128), False, False)
net.setInput(blob)
out = net.forward()
j=0
for n in net.getLayerNames():
  t = net.getLayer(j).type
  print(j,n,t)
  j += 1

# Commented out IPython magic to ensure Python compatibility.
# %cd /content
!ls -l

from google.colab import drive
drive.mount('/gdrive')
!ls -l /gdrive/'My Drive'
#!cp human_pose.onnx /gdrive/'My Drive'/lightweight-human-pose.onnx
!cp /gdrive/'My Drive'/lightweight_pose_estimation.onnx lightweight_pose_estimation.onnx
!cp /gdrive/'My Drive'/keypoints_exp.npy keypoints_exp.npy
!ls -l

!wget https://download.01.org/opencv/openvino_training_extensions/models/human_pose_estimation/checkpoint_iter_370000.pth

# Commented out IPython magic to ensure Python compatibility.
!git clone https://github.com/Daniil-Osokin/lightweight-human-pose-estimation.pytorch
# %cd lightweight-human-pose-estimation.pytorch

!pip install onnx

# Commented out IPython magic to ensure Python compatibility.
import torch

from models.with_mobilenet import PoseEstimationWithMobileNet
from modules.load_state import load_state

# %cd lightweight-human-pose-estimation.pytorch
net="models/with_mobilenet.py"
def skip_paf():
  f = open(net,"rb")
  txt = f.read()
  f.close()
  txt = txt.replace(b"return stages_output", b"return [stages_output[2]]")
  f = open(net,"wb")
  f.write(txt)
  f.close()
#skip_paf()
#!cat $net

def convert_to_onnx(net, output_name):
    input = torch.randn(1, 3, 256, 456)
    input_names = ['data']
    output_names = ['stage_1_output_1_heatmaps']

    torch.onnx.export(net, input, output_name, verbose=True, input_names=input_names, output_names=output_names)

net = PoseEstimationWithMobileNet()
checkpoint = torch.load("../checkpoint_iter_370000.pth", map_location=torch.device('cpu'))
load_state(net, checkpoint)

convert_to_onnx(net, "../human_pose.onnx")

# Commented out IPython magic to ensure Python compatibility.
# %cd /content
from google.colab.patches import cv2_imshow
import cv2, numpy as np

net = cv2.dnn.readNet("lightweight_pose_estimation.onnx")
net.setInput(np.ones((1,3,256,256),dtype=np.float32))
net.dumpToFile("human_pose_dot.txt")

!dot human_pose_dot.txt -Tpng -ohuman_pose_dot.png
#!head -c 5000 pose_dot.png
im=cv2.imread("human_pose_dot.png")
print(np.shape(im))
cv2_imshow(im)

from google.colab.patches import cv2_imshow
import cv2, numpy as np

def check(img,onnx,siz,mn):
  net = cv2.dnn.readNet(onnx)
  blob = cv2.dnn.blobFromImage(img,1./255,siz, mn, False,False)
  net.setInput(blob)
  out = net.forward()
  np.save("keypoints_exp.npy", out)
  #out = net.forward("stage_1_output_1_heatmaps")
  net.dumpToFile("pose_dot.txt")
  
  S = np.shape(out)
  print(S)
  N = S[1]
  W = S[2]
  H = S[3]
  SX = img.shape[1] / W
  SY = img.shape[0] / H
  print(SX,SY)
  j = 0
  for i in range(0,N-1):
    heat = out[0,i,:,:]
    mv,Mv,mp,Mp = cv2.minMaxLoc(heat)
    cx = int(Mp[0] * SX)
    cy = int(Mp[1] * SY)
    cv2.circle(img, (cx,cy), 3, (0,0,200), -1)
    print(i,Mp,Mv, cx,cy)
  cv2_imshow(img)


check(cv2.imread("person.png"), "lightweight_pose_estimation.onnx",(256,256),(128,128,128))

!sha1sum lightweight_pose_estimation.onnx

# Commented out IPython magic to ensure Python compatibility.
!git clone git@github.com:berak/opencv_extra.git
# %cd opencv_extra
!git checkout -b dnn_keypoints_model
!cp ../keypoints_exp.npy testdata/dnn
!ls -l testdata/dnn/*.npy

fn = "testdata/dnn/download_models.py"
f = open(fn,"rb")
txt = f.read()
f.close()
txt = txt.replace(b'https://drive.google.com/uc?export=dowload&id=1T2Kq01WXzPMrQdnEOUEiVBhwouW8Pka5', b'https://drive.google.com/uc?export=dowload&id=1--Ij_gIzCeNA488u5TA4FqWMMdxBqOji')
txt = txt.replace(b"sha='20370164b8c43aa14625c0be4e31f62b9b397ac4'", b"sha='5960f7aef233d75f8f4020be1fd911b2d93fbffc'")

f = open(fn,"wb")
f.write(txt)
f.close()
# %cat $fn

#!cat "testdata/dnn/download_models.py"
!ssh-keygen -t rsa -C "px1704@web.de"
!cat /root/.ssh/id_rsa.pub

!ssh -T git@github.com

!git config --global user.email px1704@web.de
!git config --global user.name berak

!git commit -a -m "changed keypoints model and test data"
!git push origin dnn_keypoints_model