import sys
import time
from pyqtgraph.Qt import QtCore, QtGui
import numpy as np
import pyqtgraph as pg


class App(QtGui.QMainWindow):
    def __init__(self, parent=None):
        super(App, self).__init__(parent)

        #### Create Gui Elements ###########
        self.mainbox = pg.LayoutWidget()
        self.setCentralWidget(self.mainbox)

        self.pw1 = pg.PlotWidget(name="Plot1")
        self.plot1 = self.pw1.plot()
        self.mainbox.addWidget(self.pw1, row=0, col=0)

        self.pw2 = pg.PlotWidget(name="Plot2")
        self.plot2 = self.pw2.plot()
        self.mainbox.addWidget(self.pw2, row=0, col=1)

        self.info_panel = pg.GraphicsLayoutWidget()
        self.mainbox.addWidget(self.info_panel, row=0, col=2)

        self.label = QtGui.QLabel()
        self.mainbox.addWidget(self.label, row=1, col=0)

        self.view = self.info_panel.addViewBox()
        self.view.setAspectLocked(True)
        self.view.setRange(QtCore.QRectF(0,0, 100, 100))

        #  image plot
        self.img = pg.ImageItem(border='w')
        self.view.addItem(self.img)


        #### Set Data  #####################

        self.x = np.linspace(0,50., num=100)
        self.X,self.Y = np.meshgrid(self.x,self.x)

        self.counter = 0
        self.fps = 0.
        self.lastupdate = time.time()

        #### Start  #####################
        self._update()

    def _update(self):

        self.data = np.sin(self.X/3.+self.counter/9.)*np.cos(self.Y/3.+self.counter/9.)
        self.ydata = np.sin(self.x/3.+ self.counter/9.)

        self.img.setImage(self.data)
        self.plot1.setData(self.ydata)

        now = time.time()
        dt = (now-self.lastupdate)
        if dt <= 0:
            dt = 0.000000000001
        fps2 = 1.0 / dt
        self.lastupdate = now
        self.fps = self.fps * 0.9 + fps2 * 0.1
        tx = 'Mean Frame Rate:  {fps:.3f} FPS'.format(fps=self.fps )
        self.label.setText(tx)
        QtCore.QTimer.singleShot(1, self._update)
        self.counter += 1


if __name__ == '__main__':

    app = QtGui.QApplication(sys.argv)
    thisapp = App()
    thisapp.show()
    sys.exit(app.exec_())