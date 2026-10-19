#include <QCoreApplication>
#include <QUdpSocket>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QUdpSocket s;

    if (argc >= 5 && QByteArray(argv[1]) == "send") {
        const qint64 n = s.writeDatagram(QByteArray(argv[4]),
                                        QHostAddress(QString(argv[2])),
                                        quint16(QString(argv[3]).toUShort()));
        out << "sent " << n << " bytes\n";
        QTimer::singleShot(500, &app, &QCoreApplication::quit);
        return app.exec();
    }

    if (!s.bind(QHostAddress::Any, 8008, QUdpSocket::ShareAddress)) {
        out << "bind failed\n";
        return 1;
    }
    out << "listening on 0.0.0.0:8008\n";
    QObject::connect(&s, &QUdpSocket::readyRead, [&] {
        while (s.hasPendingDatagrams()) {
            QByteArray d; d.resize(int(s.pendingDatagramSize()));
            QHostAddress from; quint16 fport;
            s.readDatagram(d.data(), d.size(), &from, &fport);
            out << "UDP from " << from.toString() << ":" << fport
                << " -> " << d << "\n";
        }
    });
    return app.exec();
}
