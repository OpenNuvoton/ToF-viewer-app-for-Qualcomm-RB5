This sample code illustrate how to use libccdtof.so
It obtain teh depth and IR iamge from it and derive 3D point cloud data.
Display it using opencv and opengl.


1) Preparation
==============

OpenCV and OpenGL installation at RB5
-------------------------------------
sudo apt update
sudo apt install libopencv-dev
sudo apt install libglfw3 libglfw3-dev
#sudo apt install freeglut3-dev

xWayland installation at RB5
----------------------------
sudo apt install xwayland
mkdir -p /root/.config
vim /root/.config/weston.ini
[core]
xwayland=true
[xwayland]
path=/usr/bin/Xwayland

Create symbolic link allow system to find the xwayland library at RB5
---------------------------------------------------------------------
cd /usr/lib/libweston-3
ln -s ../aarch64-linux-gnu/libweston-3/xwayland.so



2) Launch xWayland GUI at RB5
=============================
export XDG_RUNTIME_DIR=/usr/bin/weston_socket
export LD_LIBRARY_PATH=/usr/lib:/usr/lib/aarch64-linux-gnu/
export GDK_BACKEND=x11
weston --tty=1 --connector=29 --idle-time=0


A pure GUI with plain wallpaper will be displayed.
It have top task bar, with one icon on left hand side, and date time info at right hand side.
There is only one apps available in this xWayland GUI, i.e. "weston-terminal".
To launch xWayland apps, "weston-terminal", click on the small icon at upper left hand corner.



3) Compile at RB5
=================
./do_viewer.sh



4) Run at RB5 (Must be run at xWayland GUI prompt)
=============
./build/viewer



EOF