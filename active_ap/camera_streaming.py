import sys
import time
import socket
import collections
import numpy as np
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore, QtGui

import requests
import PIL.Image
from io import BytesIO

CAMERA_IP = "192.168.4.4" # can be improved by using mDNS
IMAGE_FRESH_INTERVAL = 20 # ms

class App(QtGui.QMainWindow):
    def __init__(self, parent=None):
        super(App, self).__init__(parent)

        #### Create Gui Elements ###########
        self.mainbox = pg.LayoutWidget()
        self.setCentralWidget(self.mainbox)

        # set up image widget
        self.img_w = pg.GraphicsLayoutWidget()
        self.mainbox.addWidget(self.img_w, row=0, col=0)
        # image view box
        self.view = self.img_w.addViewBox()
        self.view.setAspectLocked(True)
        self.view.setRange(QtCore.QRectF(0,0, 800, 600))
        #  image plot
        self.img = pg.ImageItem(border='w')
        self.view.addItem(self.img)

        # a text label widget for info dispaly
        self.label = QtGui.QLabel()
        self.mainbox.addWidget(self.label, row=1, col=0)

        #### Set Data  #####################

        self.fps = 0.
        self.lastupdate = time.time()

        # bring up camera and stream video
        self.setup_camera()

    def calculate_fps(self):
        now = time.time()
        dt = (now-self.lastupdate)
        if dt <= 0:
            dt = 0.000000000001
        fps2 = 1.0 / dt
        self.lastupdate = now
        self.fps = self.fps * 0.9 + fps2 * 0.1

    def update_label(self):
        tx = 'Mean Frame Rate:  {fps:.3f} FPS'.format(fps=self.fps )
        self.label.setText(tx)

    def setup_camera(self):
        while True:
            # set a large frame size
            cmd = {'var': 'framesize', 'val':"8"}
            resp = requests.get('http://'+ CAMERA_IP +'/control', params=cmd)
            if resp.status_code == 200:
                break
        
        self.stream_iter = requests.get('http://'+ CAMERA_IP +':81/stream', stream=True).iter_content(chunk_size=1024*1024*8)
        self.video_buf = bytes()
        # schedule the next update call
        QtCore.QTimer.singleShot(1, self.update_img)

    def update_img(self):
        try:
            self.video_buf += next(self.stream_iter)
        except:
            # schedule the next update call
            QtCore.QTimer.singleShot(IMAGE_FRESH_INTERVAL, self.update_img)
            return
        a = self.video_buf.find(b'\xff\xd8')
        b = self.video_buf.find(b'\xff\xd9')

        if a != -1 and b != -1:
            chunk_content = self.video_buf[a:b+2]
            self.video_buf = self.video_buf[b+2:]

            img_capture = PIL.Image.open(BytesIO(chunk_content))
            img_np = np.array(img_capture.rotate(-90))

            self.img.setImage(img_np)

        self.calculate_fps()
        self.update_label()

        # schedule the next update call
        QtCore.QTimer.singleShot(IMAGE_FRESH_INTERVAL, self.update_img)


if __name__ == '__main__':

    app = QtGui.QApplication(sys.argv)
    thisapp = App()
    thisapp.show()
    sys.exit(app.exec_())