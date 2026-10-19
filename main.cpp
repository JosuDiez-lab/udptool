/*
 * udptool - minimal UDP test utility for the Sailfish OS emulator.
 *
 * Built as part of the TFG "Off-network" project to verify UDP
 * connectivity between the development host and the Sailfish
 * emulator in both directions.
 *
 * Usage:
 *   udptool listen <port>
 *       Bind to 0.0.0.0:<port> (default 8008) and print every
 *       received datagram with its source address and port.
 *   udptool send <host> <port> <message>
 *       Send a single datagram and exit.
 *   udptool --help
 *       Print the usage summary.
 */

#include <QCoreApplication>
#include <QUdpSocket>
#include <QTextStream>
#include <QTimer>

// Default listen port, matches the "monp" natpf rule of the emulator.
static const quint16 kDefaultPort = 8000 + 8;

// Print usage text. Kept as a separate function so the help option
// and invalid-argument handling share one source of truth.
static void printUsage(QTextStream &out)
{
    out << "Usage:\n"
        << "  udptool listen <port>                 receive datagrams (default "
        << kDefaultPort << ")\n"
        << "  udptool send <host> <port> <message>  send one datagram\n"
        << "  udptool --help                        show this help\n" << endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QUdpSocket socket;

    // ---- help option -----------------------------------------------
    if (argc == 2 && (QByteArray(argv[1]) == "--help"
                   || QByteArray(argv[1]) == "-h")) {
        printUsage(out);
        return 0;
    }

    // ---- send mode: udptool send <host> <port> <message> ------------
    if (argc >= 2 && QByteArray(argv[1]) == "send") {
        if (argc < 5) {
            out << "udptool: send needs <host> <port> <message>\n" << endl;
            printUsage(out);
            return 1;
        }
        const QString host = QString::fromLocal8Bit(argv[2]);
        bool portOk = false;
        const quint16 port = QString::fromLocal8Bit(argv[3]).toUShort(&portOk);
        if (!portOk) {
            out << "udptool: invalid port '" << argv[3] << "'\n" << endl;
            return 1;
        }
        // An unbound QUdpSocket auto-binds an ephemeral source port
        // on first writeDatagram, which is all a one-shot send needs.
        const qint64 sent = socket.writeDatagram(QByteArray(argv[4]),
                                                  QHostAddress(host), port);
        if (sent < 0) {
            out << "send failed: " << socket.errorString() << endl;
            return 1;
        }
        out << "sent " << sent << " bytes to " << host << ":" << port << endl;
        // Give the event loop a moment to flush the datagram before
        // the process exits, otherwise the write can be discarded.
        QTimer::singleShot(500, &app, &QCoreApplication::quit);
        return app.exec();
    }

    // ---- listen mode: udptool or udptool listen <port> --------------
    quint16 listenPort = kDefaultPort;
    if (argc == 3 && QByteArray(argv[1]) == "listen") {
        bool portOk = false;
        listenPort = QString::fromLocal8Bit(argv[2]).toUShort(&portOk);
        if (!portOk) {
            out << "udptool: invalid port '" << argv[2] << "'\n" << endl;
            return 1;
        }
    } else if (argc > 2 || (argc == 2 && QByteArray(argv[1]) != "listen")) {
        // Unexpected arguments: show help instead of guessing.
        printUsage(out);
        return 1;
    }

    // Bind to every interface. Under the emulator's VirtualBox NAT the
    // forwarded packets arrive on the NAT-facing interface, not on
    // loopback, so an "Any" bind is required for host->emulator tests.
    // ShareAddress allows coexistence with a debug instance of the app.
    if (!socket.bind(QHostAddress::Any, listenPort,
                     QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        out << "bind to 0.0.0.0:" << listenPort
            << " failed: " << socket.errorString() << endl;
        return 1;
    }
    out << "listening on 0.0.0.0:" << listenPort << endl;

    // Synchronous receive loop: block until a datagram arrives, then
    // drain everything queued. waitForReadyRead works without a Qt
    // event loop, which keeps this tool free of any signal/slot or
    // lambda syntax. Stop the listener with Ctrl+C.
    for (;;) {
        if (!socket.waitForReadyRead(-1)) {
            out << "wait error: " << socket.errorString() << endl;
            return 1;
        }
        while (socket.hasPendingDatagrams()) {
            QByteArray payload;
            payload.resize(int(socket.pendingDatagramSize()));
            QHostAddress from;
            quint16 fromPort = 0;
            socket.readDatagram(payload.data(), payload.size(),
                                &from, &fromPort);
            // QHostAddress reports IPv4 senders as IPv4-mapped IPv6
            // addresses (::ffff:10.0.2.2). Convert back for output.
            const quint32 v4 = from.toIPv4Address();
            const QString fromStr = v4 != 0
                ? QHostAddress(v4).toString() : from.toString();
            out << "UDP from " << fromStr << ":" << fromPort
                << " -> " << payload << endl;
        }
    }
}
