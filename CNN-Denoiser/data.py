from load_dataset import load_exr_dataset
import torch
from torch.utils.data import TensorDataset
import numpy as np

def get_dataset():
    train_inputs = []
    train_targets = []

    for i in range(34):
        print(i)
        input = load_exr_dataset("training/4/{}_train.exr".format(i), preprocess=True, concat=True)
        target = load_exr_dataset("training/4/{}_gt.exr".format(i), preprocess=True, concat=True, target=True)
        input_v = np.flip(input, 1)[:]
        input_h = np.flip(input, 2)[:]
        input_hv = np.flip(np.flip(input, 2), 1)[:]
        target_v = np.flip(target, 1)[:]
        target_h = np.flip(target, 2)[:]
        target_hv = np.flip(np.flip(target, 2), 1)[:]

        train_inputs.append(torch.Tensor(input))
        train_inputs.append(torch.Tensor(input_h))
        train_inputs.append(torch.Tensor(input_v))
        train_inputs.append(torch.Tensor(input_hv))
        train_targets.append(torch.Tensor(target))
        train_targets.append(torch.Tensor(target_h))
        train_targets.append(torch.Tensor(target_v))
        train_targets.append(torch.Tensor(target_hv))

        train_inputs = torch.stack(train_inputs, 0)
        train_targets = torch.stack(train_targets, 0)

        train_dataset = TensorDataset(train_inputs, train_targets)
        test_dataset = TensorDataset(train_inputs, train_targets)
        return train_dataset, test_dataset