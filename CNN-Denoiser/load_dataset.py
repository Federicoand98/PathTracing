import OpenEXR
import Imath
import random
import numpy as np
import matplotlib.pyplot as plt

def load_exr_dataset(filename, preprocess=False, concat=False, target=False):
    infile = OpenEXR.InputFile(filename)

    color = get_layer(infile, 'Color')
    normal = get_layer(infile, 'Normal')
    albedo = get_layer(infile, 'Albedo')
    depth = get_layer(infile, 'Depth')
    color_var = get_layer(infile, 'ColorVar')
    normal_var = get_layer(infile, 'NormalVar')
    albedo_var = get_layer(infile, 'AlbedoVar')
    depth_var = get_layer(infile, 'DepthVar')

    if preprocess:
        depth /= np.max(depth) + 0.00001
        depth_var /= np.max(depth_var) + 0.00001
        albedo_var /= np.max(albedo_var) + 0.00001
        color_var /= np.max(color_var) + 0.00001
        normal_var /= np.max(normal_var) + 0.00001

    if not target:
        data = [color, normal, albedo, depth, color_var, normal_var, albedo_var, depth_var]
    else:
        data = [np.clip(color, 0, 1)]

    if concat:
        data = np.concatenate(data, axis=1)
        data = np.swapaxes(data, 0, 2)
        return data
    
    return data

def get_layer(infile, layer_name):
    # Return a np array with all channels of a single layer in the EXR file with the matching layer name

    channel_names = []
    for layer in infile.header()['channels']:
        if layer_name+'.' in layer:                         # add . to end of layer_name so we don't get layers that start with the same prefix too
            channel_names.append(layer)

    if not channel_names:
        print('Warning: Layer \'%s\' was not found', layer_name)
        return None
    
    if channel_names:
        channel_names = sorted(channel_names)

        if channel_names[0].split('.')[-1] == 'B':          # BGR to RGB
            channel_names = [channel_names[2], channel_names[1], channel_names[0]]
    
    dw = infile.header()['dataWindow']
    size = (dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1)
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    data = np.zeros((size[1], size[0], len(channel_names)))

    for i, name in enumerate(channel_names):
        data[:, :, i] = np.fromstring(infile.channel(name, pt), dtype=np.float32).reshape(size[1], size[0])
    
    return data

def get_score(patch):
    return np.sum(patch[:, :, 10]) + np.sum(patch[:, :, 11])

if __name__ == "__main__":
    print("Testing load_exr_dataset")
    x = load_exr_dataset("training/0_gt.exr", preprocess=True, concat=True, target=True)
    print(x.shape)
    plt.imshow(np.swapaxes(x, 0, 2))
    plt.show()