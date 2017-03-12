/*
 * mian.cpp
 *
 *  Created on: Feb 26, 2017
 *      Author: aes
 */

extern "C"
{
#include "c64.h"
}

#include <QObject>
#include <QApplication>
#include <QImage>
#include <QMainWindow>
#include <QLabel>
#include <QThread>
#include <QGraphicsScene>
#include <QTimer.h>
#include <stdint.h>
#include <QKeyEvent>
#include <QMutex>
#include <QUrl>
#include <QMimeData>

QMutex pix_buf_lock;
extern int key_matrix(int keycode);



class C64 : public QThread {
  Q_OBJECT
public:
  void sceeen_draw_done() {
    emit update();
  }

signals:
  void update();

protected:
  void run() {
    c64_init();

    while(1) {
      pix_buf_lock.lock();
      c65_run_frame();
      pix_buf_lock.unlock();
    }
  }
};



class C64* the_c64;

extern "C" void vic_screen_draw_done() {
  the_c64->sceeen_draw_done();
}

class Window : public QMainWindow
{
 Q_OBJECT
 public:
  Window(QWidget *parent = 0) {
    label = new QLabel;
    label->resize(512,403);
    resize(label->size());

    setAcceptDrops(true);
    setCentralWidget(label);
    setFocusPolicy(Qt::StrongFocus);
    the_c64 = &c64;
    c64.start();
    connect(&c64, SIGNAL(update()), this, SLOT(update_screen()));
  }

  private slots:
  void update_screen() {
    pix_buf_lock.lock();
    QImage screen ((uchar*)pixelbuf,512,403,QImage::Format_RGBA8888);
    label->setPixmap(QPixmap::fromImage(screen));
    pix_buf_lock.unlock();
    update();
  }

  protected:
  void keyPressEvent(QKeyEvent *event) {
    c64_key_press( key_matrix(event->key()),1);
  }
  void keyReleaseEvent(QKeyEvent *event) {
    c64_key_press( key_matrix(event->key()),0);
  }



  void dropEvent(QDropEvent* event)
   {
     const QMimeData* mimeData = event->mimeData();

     // check for our needed mime type, here a file or a list of files
     if (mimeData->hasUrls())
     {
       QList<QUrl> urlList = mimeData->urls();

       // extract the local paths of the files
       for (int i = 0; i < urlList.size() && i < 32; ++i)
       {
         c64_load_prg(urlList.at(i).toLocalFile().toStdString().c_str());
         break;
       }
     }
   }


  void dragEnterEvent(QDragEnterEvent* event)
  {
    // if some actions should not be usable, like move, this code must be adopted
    event->acceptProposedAction();
  }

  void dragMoveEvent(QDragMoveEvent* event)
  {
    // if some actions should not be usable, like move, this code must be adopted
    event->acceptProposedAction();
  }

  void dragLeaveEvent(QDragLeaveEvent* event)
  {
    event->accept();
  }

 private:
 QLabel *label;
 C64 c64;
};



#include "main.moc"

int main(int argc, char **argv)
{
 QApplication app (argc, argv);
 Window w;
 w.show();


 return app.exec();
}