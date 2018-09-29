# D/D2MAC-Eurocrypt

VirtualDub plug-in to simulate D/D2MAC transmission standard mainly used in Europe on satellite and cable networks. 

Unlike PAL, MAC used time delay synchronisation with data, chroma and luma transmitted separately. Audio was essentially NICAM coded within the data area. The only difference between DMAC and D2MAC was the bandwidth requirement for cable networks with D2MAC having half the bandwidth of DMAC.

Encryption used here was line cut and rotate, similar to Videocrypt. There were two modes: single cut, where only chroma part was cut and rotated along with luma and double cut, where chroma and luma were cut and rotated separately.

## Unencrypted MAC transmission

![MAC Clear](https://filmnet.plus/images/VDMACClear.jpeg)

## Single cut encryption MAC transmission

![MAC Single Cut](https://filmnet.plus/images/VDMACSingle.jpeg)

## Double cut encryption MAC transmission

![MAC double cut](https://filmnet.plus/images/VDMACDouble.jpeg)

