import numpy as np
import cv2
from patchify import patchify, unpatchify
import tensorflow as tf
import matplotlib.pyplot as plt
import sys


def patches(img, patch_size):
    patches = patchify(img, (patch_size, patch_size, 3), step=patch_size)
    return patches

def prediction_tflite(img, model, input_details, output_details):
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = cv2.resize(img,(1024,1024))
    img = img.astype("float32") / 255.0

    img_patches = patches(img,256)

    nsy=[]
    for i in range(4):
        for j in range(4):
            nsy.append(img_patches[i][j][0])

    nsy = np.array(nsy)
    pred=[]

    for patch in nsy:
        model.set_tensor(input_details[0]['index'], tf.expand_dims(patch,axis=0))
        model.invoke()
        tflite_model_predictions = model.get_tensor(output_details[0]['index'])
        pred.append(tflite_model_predictions)

    pred_img = np.reshape(pred,(4,4,1,256,256,3))
    pred_img = unpatchify(pred_img, img.shape)
    return pred_img

def prediction(img, model):
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = cv2.resize(img,(1024,1024))
    img = img.astype("float32") / 255.0

    img_patches = patches(img,256)

    nsy=[]
    for i in range(4):
        for j in range(4):
            nsy.append(img_patches[i][j][0])

    nsy = np.array(nsy)

    pred_img = model.predict(nsy)
    pred_img = np.reshape(pred_img,(4,4,1,256,256,3))
    pred_img = unpatchify(pred_img, img.shape)
    return pred_img


print("Loading model...")
CBDNet = tf.keras.models.load_model('../../CNN-Denoiser/models/CBDNet')
print("Model loaded")

path = sys.argv[1]
input_img = cv2.imread("C:\\Users\\Federico\\Documents\\Sviluppo\\Git\\PathTracing\\PathTracing\\res\\screenshots\\0.png")

if input_img is None:
    print('Impossibile leggere l\'immagine da: ' + '../' + path)
else:
    print('Denoising image...')
    output_img = prediction(input_img, CBDNet)

    out_name = path.split('.')[0] + '_denoised.' + path.split('.')[1]
    output_img = cv2.cvtColor(output_img, cv2.COLOR_RGB2BGR)
    cv2.imwrite(out_name, output_img * 255.0)

    print('Denoised image saved as ' + out_name)