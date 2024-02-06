from tensorflow import keras
import numpy as np

class Dataloader(keras.utils.Sequence):
    def __init__(self, X, y, batch_size=1, shuffle=False):
        self.X = X
        self.y = y
        self.batch_size = batch_size
        self.shuffle = shuffle
        self.indexes = np.arange(len(X))

    def __getitem__(self, i):
        batch_x = self.X[i * self.batch_size : (i + 1) * self.batch_size]
        batch_y= self.y[i * self.batch_size : (i + 1) * self.batch_size]
        return tuple((batch_x, batch_y))

    def __len__(self):
        return len(self.indexes)

    def on_epoch_end(self):
        if self.shuffle:
            self.indexes = np.random.permutation(self.indexes)
