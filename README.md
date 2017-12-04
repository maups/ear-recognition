# Employing Fusion of Learned and Handcrafted Features for Unconstrained Ear Recognition
Authors: Earnest E. Hansley, Mauricio Pamplona Segundo, Sudeep Sarkar

Models and demo code for http://arxiv.org/abs/1710.07662

# Models
- Side classifier: [https://drive.google.com/open?id=17Km_uNnK135w94HlXWXzUW0hYxRj_9-W](https://drive.google.com/open?id=17Km_uNnK135w94HlXWXzUW0hYxRj_9-W)
- Landmark detector (1st stage): [https://drive.google.com/open?id=1WhG3Jq5qzYe7hGL-Q_oHcBI_iaxaRPcO](https://drive.google.com/open?id=1WhG3Jq5qzYe7hGL-Q_oHcBI_iaxaRPcO)
- Landmark detector (2nd stage): [https://drive.google.com/open?id=12oM0NPmJhDKI_m1GakvTFtV3Uaxk9wUo](https://drive.google.com/open?id=12oM0NPmJhDKI_m1GakvTFtV3Uaxk9wUo)
- Description extractor: [https://drive.google.com/open?id=1BnUX8cVrjYBtpD8ESnwb5b6Wmiuknl3S](https://drive.google.com/open?id=1BnUX8cVrjYBtpD8ESnwb5b6Wmiuknl3S)

# Requirements

This code only requires the master branch of OpenCV to work. You can install it locally with the following sequence of steps without making any modifications in a different OpenCV version previously installed in your system (Ubuntu 16.04):

```
$ sudo apt-get install build-essential
$ sudo apt-get install cmake git libgtk2.0-dev pkg-config libavcodec-dev libavformat-dev libswscale-dev
$ sudo apt-get install python-dev python-numpy libtbb2 libtbb-dev libjpeg-dev libpng-dev libtiff-dev libjasper-dev libdc1394-22-dev
$ cd ~/your_choice/
$ git clone https://github.com/opencv/opencv.git
$ git clone https://github.com/opencv/opencv_contrib.git
$ mkdir opencv_install
$ cd opencv
$ mkdir build
$ cd build
$ cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=../../opencv_install/ -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules/ ..
$ make -j 4
$ make install
```

# Compiling and running

How to compile and run using a local OpenCV installation:

```
$ g++ -std=c++11 demo.cpp -o demo `pkg-config --libs --cflags ~/your_choice/opencv_install/lib/pkgconfig/opencv.pc`
$ export LD_LIBRARY_PATH=~/your_choice/opencv_install/lib/
$ ./demo your_image.png
```
