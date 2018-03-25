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
#include <QTimer>
#include <stdint.h>
#include <QKeyEvent>
#include <QMutex>
#include <QUrl>
#include <QMimeData>
#include <QAudioOutput>
#include <QBuffer>
/*
extern "C" {
 #include "sid_wrapper.h"
}*/

extern "C" {
 #include "mysid.h"
}

QMutex pix_buf_lock;
extern int key_matrix(int keycode);

int16_t audio_buffer[SAMPLE_BUFFER_SIZE];

class C64 : public QThread {
  Q_OBJECT
public:
  void sceeen_draw_done() {
    emit update();
  }
  void audio_ready() {
    emit update_audio();
  }

  void stop( ) {
    running = 0;
    wait();
  }

signals:
  void update();
  void update_audio();

protected:
  void run() {
    running = 1;
    c64_init();

    while(running) {
      pix_buf_lock.lock();
      c65_run_frame();
      pix_buf_lock.unlock();
    }
  }

  int running;
};



class C64* the_c64;

extern "C" void vic_screen_draw_done() {
  the_c64->sceeen_draw_done();
}

extern "C" void sid_audio_ready(int16_t* data, int n) {
  memcpy(audio_buffer,data,n*sizeof(int16_t));

  the_c64->audio_ready();
}

class Window : public QMainWindow
{
 Q_OBJECT

 public:
  Window(QWidget *parent = 0)  {
    label = new QLabel;
    label->resize(512,312);
    resize(label->size());

    setAcceptDrops(true);
    setCentralWidget(label);
    setFocusPolicy(Qt::StrongFocus);
    the_c64 = &c64;
    c64.start();
    joystick_mode=KEYBOARD;

    QAudioFormat format;

    //format.setSampleRate(15625);
    format.setSampleRate(15625-500);

    //format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    memset(audio_buffer,0,sizeof(audio_buffer));
    m_audioOutput = new QAudioOutput(format, this);
    m_audioOutput->setBufferSize(sizeof(audio_buffer)*2);
    m_audio_buffer = m_audioOutput->start( );
    m_audio_buffer->write(QByteArray((const char*)audio_buffer,sizeof(audio_buffer)));

    connect(&c64, SIGNAL(update()), this, SLOT(update_screen()));
    connect(&c64, SIGNAL(update_audio()), this, SLOT(play_audio_buffer()));

  }

  ~Window() {
    c64.stop();
  }

  private slots:
  void update_screen() {
    //pix_buf_lock.lock();
    QImage screen ((uchar*)pixelbuf,512,312,QImage::Format_RGBA8888);
    label->setPixmap(QPixmap::fromImage(screen));
    //pix_buf_lock.unlock();
    update();
  }

  void play_audio_buffer() {
    pix_buf_lock.lock();
    m_audio_buffer->write(QByteArray((const char*)audio_buffer,sizeof(audio_buffer)));
    pix_buf_lock.unlock();
  }


  protected:

  int getJoystickKey(int code) {
    switch(code) {
   case Qt::Key_Up:
     return 0;
    case Qt::Key_Down:
      return 1;
    case Qt::Key_Left:
      return 2;
    case Qt::Key_Right:
      return 3;
    case Qt::Key_Space:
      return 4;
    default:
      return -1;
    }
  }


  void input(QKeyEvent *event, bool state) {
    switch(joystick_mode) {
    case KEYBOARD:
      c64_key_press( key_matrix(event->key()),state);
      break;
    case JOY1:
      c64_joy1_press(getJoystickKey(event->key()), state);
      break;
    case JOY2:
      c64_joy2_press(getJoystickKey(event->key()), state);
      break;
    }

  }
  void keyPressEvent(QKeyEvent *event) {
    const char* input_dev[] = {"keyboard","joystick1","joystick2"};

    if(event->key() == Qt::Key_Alt) {
      joystick_mode = (joystick_mode_t)(joystick_mode + 1);

      if(joystick_mode > JOY2) joystick_mode=KEYBOARD;
      qDebug("Switched to %s mode", input_dev[joystick_mode]);
    }

    input(event,true);

  }
  void keyReleaseEvent(QKeyEvent *event) {
    input(event,false);
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
         QString filename = urlList.at(i).toLocalFile();
         if(filename.endsWith(".prg")) c64_load_prg(filename.toStdString().c_str());
         if(filename.endsWith(".tap")) open_tape(filename.toStdString().c_str());


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
 QAudioOutput* m_audioOutput;
 QIODevice* m_audio_buffer;

 typedef enum {KEYBOARD,JOY1,JOY2} joystick_mode_t ;

 joystick_mode_t joystick_mode;
};



#include "main.moc"

int main(int argc, char **argv)
{
 QApplication app (argc, argv);
 Window w;
 w.show();


 return app.exec();
}
