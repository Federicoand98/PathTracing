import torch
from torch import nn
from torch.nn import init
import torch.nn.functional as F

class ResidualBlock(nn.Module):
    def __init__(self, in_size, out_size):
        super(ResidualBlock, self).__init__()
        self.relu = nn.ReLU()
        self.res_conv = nn.Conv2d(in_size, out_size, kernel_size=3, stride=2, padding=1, dilation=1, groups=1, bias=True)
        self.res_bn = nn.BatchNorm2d(out_size)

        self.conv1 = nn.Conv2d(in_size, out_size, kernel_size=3, stride=2, padding=1, dilation=1, groups=1, bias=True)
        self.bn1 = nn.BatchNorm2d(out_size)
        self.conv2 = nn.Conv2d(out_size, out_size, kernel_size=3, stride=1, padding=1, dilation=1, groups=1, bias=True)
        self.bn2 = nn.BatchNorm2d(out_size)
    
    def forward(self, x):
        residual = self.res_conv(x)
        residual = self.relu(residual)
        residual = self.res_bn(residual)

        x = self.conv1(x)
        x = self.relu(x)
        x = self.bn1(x)
        x = self.conv2(x)
        x = self.relu(x)
        x = self.bn2(x)

        x = x + residual
        return x

class DenoiseCNN(nn.Module):
    def __init__(self):
        super(DenoiseCNN, self).__init__()
        self.block1 = ResidualBlock(14, 32)
        self.block2 = ResidualBlock(32, 64)
        self.block3 = ResidualBlock(64, 128)
        self.block4 = ResidualBlock(128, 256)
        self.block5 = ResidualBlock(256, 512)
        self.block6 = ResidualBlock(512, 1024)

        self.relu = nn.ReLU()

        self.lat_6 = nn.Conv2d(1024, 32, kernel_size=1, stride=1, padding=0)
        self.backwards_65 = nn.Conv2d(32, 32, kernel_size=3, stride=2, padding=1)
        self.lat_5 = nn.Conv2d(512, 32, kernel_size=1, stride=1, padding=0)
        self.backwards_54 = nn.Conv2d(32, 32, kernel_size=3, stride=2, padding=1)
        self.lat_4 = nn.Conv2d(256, 32, kernel_size=1, stride=1, padding=0)
        self.backwards_43 = nn.Conv2d(32, 32, kernel_size=3, stride=2, padding=1)
        self.lat_3 = nn.Conv2d(128, 32, kernel_size=1, stride=1, padding=0)
        self.backwards_32 = nn.Conv2d(32, 32, kernel_size=3, stride=2, padding=1)
        self.lat_2 = nn.Conv2d(64, 32, kernel_size=1, stride=1, padding=0)
        self.backwards_21 = nn.Conv2d(32, 32, kernel_size=3, stride=2, padding=1)
        self.lat_1 = nn.Conv2d(32, 32, kernel_size=1, stride=1, padding=0)
        self.backwards_10 = nn.Conv2d(32, 32, kernel_size=3, stride=2, padding=1)

        self.lat_0 = nn.Conv2d(14, 32, kernel_size=1, stride=1, padding=0)
        self.rgb_conv = nn.Conv2d(32, 3, kernel_size=3, stride=1, padding=1, dilation=1, groups=1, bias=True)
    
    def _upsample_add(self, x, y):
        _, _, h, w = y.size()
        return F.upsample(x, size=(h, w), mode='bilinear') + y

    def forward(self, x):
        # Forward pass
        raw1 = self.block1(x)
        raw2 = self.block2(raw1)
        raw3 = self.block3(raw2)
        raw4 = self.block4(raw3)
        raw5 = self.block5(raw4)
        raw6 = self.block6(raw5)

        #Backwards pass
        rep = self.relu(self.lat_6(raw6))

        rep = self.relu(self.backwards_65(rep))
        rep = self._upsample_add(rep, self.relu(self.lat_5(raw5)))

        rep = self.relu(self.backwards_54(rep))
        rep = self._upsample_add(rep, self.relu(self.lat_4(raw4)))

        rep = self.relu(self.backwards_43(rep))
        rep = self._upsample_add(rep, self.relu(self.lat_3(raw3)))

        rep = self.relu(self.backwards_32(rep))
        rep = self._upsample_add(rep, self.relu(self.lat_2(raw2)))

        rep = self.relu(self.backwards_21(rep))
        rep = self._upsample_add(rep, self.relu(self.lat_1(raw1)))

        rep = self.relu(self.backwards_10(rep))
        rep = self._upsample_add(rep, self.relu(self.lat_0(x)))
        
        rep = self.rgb_conv(rep)
        return torch.clamp(rep, 0, 1)

    def _initialize_weights(self):
        for m in self.modules():
            if isinstance(m, nn.Conv2d):
                init.orthogonal(m.weight)