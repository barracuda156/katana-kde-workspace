
#include <QtTest>
#include <QtCore/qnamespace.h>

#include <QObject>
#include "ksysguard/ksgrd/SensorManager.h"
#include "ksysguard/ksgrd/SensorAgent.h"
#include "ksysguard/ksgrd/SensorClient.h"
#include <QDebug>
class SensorClientTest;

class TestKsysguardd : public QObject
{
    Q_OBJECT
    private slots:
        void init();
        void cleanup();
        void initTestCase();
        void cleanupTestCase();

        void testSetup();
        void testFormatting_data();
        void testFormatting();
        void testQueueing();
    private:
        KSGRD::SensorManager manager;
        SensorClientTest *client;
        QSignalSpy *hostConnectionLostSpy;
        QSignalSpy *updateSpy;
        QSignalSpy *hostAddedSpy;
        int nextId;
};
struct Answer {
    Answer() {
        id = -1;
        isSensorLost = false;
    }
    int id;
    QList<QByteArray> answer;
    bool isSensorLost;
};
class SensorClientTest : public KSGRD::SensorClient
{
public:
    SensorClientTest() {
        isSensorLost = false;
        haveAnswer = false;
    }
    virtual void answerReceived( int id, const QList<QByteArray>& answer_ ) {
        Answer answer;
        answer.id = id;
        answer.answer = answer_;
        answers << answer;
        haveAnswer = true;
    }
    virtual void sensorLost(int id)
    {
        Answer answer;
        answer.id = id;
        answer.isSensorLost = true;
        answers << answer;
        isSensorLost = true;
    }
    bool isSensorLost;
    bool haveAnswer;
    QList<Answer> answers;
};
