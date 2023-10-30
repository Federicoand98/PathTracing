import numpy as np
import cv2
from patchify import patchify, unpatchify
import tensorflow as tf
import matplotlib as plt
from skimage.metrics import peak_signal_noise_ratio as psnr
from skimage.metrics import structural_similarity as ssim

def patches(img, patch_size):
    patches = patchify(img, (patch_size, patch_size, 3), step=patch_size)
    return patches

#Custom function to get denoised image prediction for noisy images
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


#Custom function to get denoised image prediction for noisy images on quantized models using tflite
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
     
#Custom function to plot/visualize noisy, ground truth and predicted images
def visualize(sample,model):
    fig,ax = plt.subplots(len(sample),3,figsize=(30,30))

    for i in range(len(sample)):
        path = sample['Ground Truth Images'].iloc[i]
        test_img_gt = cv2.imread(path)
        test_img_gt = cv2.cvtColor(test_img_gt, cv2.COLOR_BGR2RGB)
        test_img_gt = cv2.resize(test_img_gt,(512,512))
        test_img_gt = test_img_gt.astype("float32") / 255.0

        path = sample['Noisy Images'].iloc[i]
        test_img_nsy = cv2.imread(path)
        pred_img = prediction(test_img_nsy,model)
        pred_img = cv2.resize(pred_img,(512,512))

        test_img_nsy = cv2.cvtColor(test_img_nsy, cv2.COLOR_BGR2RGB)
        test_img_nsy = cv2.resize(test_img_nsy,(512,512))
        test_img_nsy = test_img_nsy.astype("float32") / 255.0

        ax[i][0].imshow(test_img_nsy)
        ax[i][0].get_xaxis().set_visible(False)
        ax[i][0].get_yaxis().set_visible(False)
        ax[i][0].title.set_text("Noisy Image")

        ax[i][1].imshow(test_img_gt)
        ax[i][1].get_xaxis().set_visible(False)
        ax[i][1].get_yaxis().set_visible(False)
        ax[i][1].title.set_text("Ground Truth Image")

        ax[i][2].imshow(pred_img)
        ax[i][2].get_xaxis().set_visible(False)
        ax[i][2].get_yaxis().set_visible(False)
        ax[i][2].title.set_text("Predicted Image")

#Custom function that computes the psnr and ssim values for images
def psnr_and_ssim(X_test,y_test,model,model_type='Normal'):
    psnr_nsy = 0.0
    psnr_de_nsy = 0.0
    ssim_nsy = 0.0
    ssim_de_nsy = 0.0

    for i in range(len(X_test)):
        #getting the noisy images
        path = X_test.iloc[i]
        nsy = cv2.imread(path)  

        #getting the predicted images
        if model_type == 'Quantized': 
            pred = prediction_tflite(nsy,model)
        else:
            pred = prediction(nsy,model)

        #getting the ground truth images
        path = y_test.iloc[i]
        gt = cv2.imread(path)         
        gt = cv2.cvtColor(gt, cv2.COLOR_BGR2RGB)

        #Resizing the images
        gt = cv2.resize(gt,(1024,1024))
        nsy = cv2.resize(nsy,(1024,1024))

        #Normalizing the images
        gt = gt.astype("float32") / 255.0
        nsy = nsy.astype("float32") / 255.0

        #Computing psnr and ssim for test images
        psnr_nsy += psnr(gt,nsy)
        psnr_de_nsy += psnr(gt,pred)
        ssim_nsy += ssim(gt,nsy,multichannel=True,data_range=nsy.max() - nsy.min())
        ssim_de_nsy += ssim(gt,pred,multichannel=True,data_range=pred.max() - pred.min())

    psnr_nsy = psnr_nsy/len(X_test)
    psnr_de_nsy = psnr_de_nsy/len(X_test)
    ssim_nsy = ssim_nsy/len(X_test)
    ssim_de_nsy = ssim_de_nsy/len(X_test)

    return psnr_nsy, psnr_de_nsy,ssim_nsy,ssim_de_nsy