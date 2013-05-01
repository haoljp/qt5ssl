

#include <qglobal.h>
// To prevent windows system header files from re-defining min/max
#define NOMINMAX 1
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#define SOCKET int
#define INVALID_SOCKET -1
#endif

#include <QtTest/QtTest>

#ifndef Q_OS_WIN
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#include "qsslserver.h"
#include "SslSocket.h"
#include <qcoreapplication.h>
#include <qsslsocket.h>
#include <qhostaddress.h>
#include <qprocess.h>
#include <qstringlist.h>
#include <qplatformdefs.h>
#include <qhostinfo.h>


#include <QNetworkSession>
#include <QNetworkConfiguration>
#include <QNetworkConfigurationManager>

class tst_QSslServer : public QObject
{
  Q_OBJECT

public:
  tst_QSslServer();
  virtual ~tst_QSslServer();


public slots:
  void slotAcceptError(QAbstractSocket::SocketError error);
  void initTestCase_data();
  void initTestCase();
  void init();
  void cleanup();
private slots:
  void clientServerLoop();
  void crashTests();

private:
  void ignoreHostnameAndSelfSignedErr(SslSocket *client);
#ifndef QT_NO_BEARERMANAGEMENT
  QNetworkSession *networkSession;
#endif
  QSslCertificate signedSelfCert;
  QSslKey privateKey;
};


tst_QSslServer::tst_QSslServer()
{
  qRegisterMetaType< QList<QSslError> >("QList<QSslError>");
}

tst_QSslServer::~tst_QSslServer()
{
}

void tst_QSslServer::slotAcceptError(QAbstractSocket::SocketError error)
{
  qDebug() << "sslserver slot Accept Error:\t" << error;
}

void tst_QSslServer::ignoreHostnameAndSelfSignedErr(SslSocket *client)
{
  QSslError selfSignedError(QSslError::SelfSignedCertificate, signedSelfCert);
  QSslError hostNameError(QSslError::HostNameMismatch, signedSelfCert);
  QList<QSslError> expectedSslErrors;
  expectedSslErrors.append(selfSignedError);
  expectedSslErrors.append(hostNameError);
  client->ignoreSslErrors(expectedSslErrors);
}

void tst_QSslServer::initTestCase_data()
{
}

void tst_QSslServer::initTestCase()
{
  //#ifndef QT_NO_BEARERMANAGEMENT
  //    QNetworkConfigurationManager man;
  //    networkSession = new QNetworkSession(man.defaultConfiguration(), this);
  //    networkSession->open();
  //    QVERIFY(networkSession->waitForOpened());
  //#endif

  QFile certFile("d:/server.csr");
  QVERIFY(certFile.open(QIODevice::ReadOnly));
  signedSelfCert = QSslCertificate (&certFile);
  QVERIFY(!signedSelfCert.isNull());
  QVERIFY(signedSelfCert.isValid());
  certFile.close();

  QFile privateFile("d:/server.key");
  QVERIFY(privateFile.open(QIODevice::ReadOnly));
  privateKey = QSslKey(&privateFile, QSsl::Rsa);
  QVERIFY(!privateKey.isNull());
  privateFile.close();
}

void tst_QSslServer::init()
{
}

void tst_QSslServer::cleanup()
{
}

//----------------------------------------------------------------------------------
void tst_QSslServer::clientServerLoop()
{
  QSslServer server;
  server.setPrivateKey(privateKey);
  server.setLocalCertificate(signedSelfCert);
  connect(&server, SIGNAL(acceptError(QAbstractSocket::SocketError)),
          SLOT(slotAcceptError(QAbstractSocket::SocketError)));

  qRegisterMetaType<SslSocket *>("SslSocket*");

  QSignalSpy spy(&server, SIGNAL(newSslConnection(SslSocket*)));

  QVERIFY(!server.isListening());
  QVERIFY(!server.hasPendingConnections());
  QVERIFY(server.listen(QHostAddress("127.0.0.1"), 8888));
  QVERIFY(server.isListening());

  SslSocket client;
  client.setObjectName("clientToServer");
  client.setPrivateKey(privateKey);
  client.setLocalCertificate(signedSelfCert);
  ignoreHostnameAndSelfSignedErr(&client);
  client.addCaCertificate(signedSelfCert);

  client.connectToHostEncrypted("localhost", 8888);
  QVERIFY(client.waitForConnected(5000));
  QVERIFY(server.waitForNewConnection(5000));

  QVERIFY(server.hasSslPendingConnections());
  QCOMPARE(spy.count(), 1);

  SslSocket *serverSocket = server.nextSslPendingConnection();
  QVERIFY(serverSocket != 0);
  serverSocket->setObjectName("serverToClient");
  if (!serverSocket->isEncrypted()) {
    if (!serverSocket->waitForEncrypted(5000)) {
      QWARN(serverSocket->errorString().toLatin1());
      qDebug() << int(serverSocket->error());
      qDebug() << server.errorString();
    }
  }
  if (!client.isEncrypted())
    qDebug()<< "client is not Encrypted" , client.waitForEncrypted(20000);


  QVERIFY(serverSocket->write("Greetings, client!\n", 19) == 19);
  serverSocket->flush();
  serverSocket->waitForBytesWritten();

  QVERIFY(client.waitForReadyRead(5000));
  QByteArray arr = client.readLine();
  QCOMPARE(arr.constData(), "Greetings, client!\n");

  QVERIFY(client.write("Well, hello to you!\n", 20) == 20);
  client.flush();

  QVERIFY(serverSocket->waitForReadyRead(5000));
  arr = serverSocket->readAll();
  QCOMPARE(arr.constData(), "Well, hello to you!\n");
}

//----------------------------------------------------------------------------------
void tst_QSslServer::crashTests()
{
  QSslServer server;
  server.close();
  QVERIFY(server.listen());
}


QTEST_MAIN(tst_QSslServer)
#include "tst_qsslserver.moc"
