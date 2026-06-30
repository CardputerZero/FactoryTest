# CardputerZero Factory Test Application

This repository holds the factory test application of CardputerZero.

## Get started:

+ Download the debian release

```
wget https://github.com/CardputerZero/FactoryTest/releases/download/0.1.0/FactoryTest_0.1.0_m5stack1_arm64.deb
```

+ Install the package

```
sudo dpkg -i FactoryTest_0.1.0_m5stack1_arm64.deb
```

+ Fix dependency issue

```
sudo apt -f install
```


## Manually build

TODO

## Changelog

### 0.1.0 - 2026-06-29

- 2026-06-26 17:05 +0800 - 2215c33 - feat: add categorized drawer navigation and performance test flow
- 2026-06-25 14:13 +0800 - 1ac24e1 - refactor: decouple the implementations in connectivity service, implement the UART service with libserialport
- 2026-06-24 09:22 +0800 - 0f89754 - feat: expand factory test hardware coverage and navigation actions
