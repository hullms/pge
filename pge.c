#include <QApplication>
#include <QMainWindow>
#include <QStackedWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QShortcut>
#include <QStyle>
#include <QKeyEvent>
#include <QFileInfo>
#include <QDebug>
#include <QMetaObject>
#include <QUrl>
// Qt Multimedia (Qt6)
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <gpiod.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <functional>
// UART 
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <mutex>
#include <poll.h>
#include <cstring>

static const int MAX_PAGES = 5;

// === STX/ETX (doivent matcher STM) ===
static constexpr uint8_t STX = 0x02;
static constexpr uint8_t ETX = 0x03;

// === Timeouts ===
static constexpr int UART_RESPONSE_TIMEOUT_MS = 4000;
static constexpr int UART_MAX_PAYLOAD = 16384;

enum class LogFilter { All, Only15, Only25, Other };

static uint8_t makeFrame(uint8_t cmd2, uint8_t param2);
static QString hexByte(uint8_t b);

static QString logPath() {
    QString dir = QDir::homePath() + "/pge";
    QDir().mkpath(dir);
    return dir + "/pgelogs.txt";
}

static void write_log_line(int code, const QString& text) {
    QFile f(logPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;

    QTextStream out(&f);
    const QString ts = QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss");
    out << "[" << ts << "] (" << code << ") " << text << "\n";
}

static QStringList load_last_logs_limited(int maxKeep) {
    if (maxKeep <= 0) return {};

    QFile f(logPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};

    QTextStream in(&f);

    QStringList ring;
    ring.reserve(maxKeep);

    int start = 0;
    int count = 0;

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (count < maxKeep) {
            ring.push_back(line);
            count++;
        } else {
            ring[start] = line;
            start = (start + 1) % maxKeep;
        }
    }

    if (count == maxKeep && start != 0) {
        QStringList ordered;
        ordered.reserve(maxKeep);
        for (int i = 0; i < maxKeep; i++) {
            int idx = (start + i) % maxKeep;
            ordered.push_back(ring[idx]);
        }
        return ordered;
    }

    return ring;
}

// =========================== UART ===========================
class UartLink {
public:
    explicit UartLink(const QString& dev = "/dev/serial0", int baud = 115200)
        : device(dev), baudrate(baud) {}

    ~UartLink() { closePort(); }

    bool openPort() {
        std::lock_guard<std::mutex> lk(mu);
        closePort_nolock();

        fd = ::open(device.toLocal8Bit().constData(), O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0) {
            qDebug().noquote() << "UART: open failed" << device << "errno=" << errno;
            return false;
        }

        termios tty{};
        if (tcgetattr(fd, &tty) != 0) {
            qDebug().noquote() << "UART: tcgetattr failed errno=" << errno;
            closePort_nolock();
            return false;
        }

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        tty.c_lflag = 0;
        tty.c_oflag = 0;

        tty.c_cc[VMIN]  = 0;
        tty.c_cc[VTIME] = 1; // 0.1s

        speed_t sp = B115200;
        if (baudrate == 9600) sp = B9600;
        else if (baudrate == 19200) sp = B19200;
        else if (baudrate == 38400) sp = B38400;
        else if (baudrate == 57600) sp = B57600;
        else sp = B115200;

        cfsetispeed(&tty, sp);
        cfsetospeed(&tty, sp);

        if (tcsetattr(fd, TCSANOW, &tty) != 0) {
            qDebug().noquote() << "UART: tcsetattr failed errno=" << errno;
            closePort_nolock();
            return false;
        }

        qDebug().noquote() << "UART: OK device=" << device << "baud=" << baudrate;
        return true;
    }

    void closePort() {
        std::lock_guard<std::mutex> lk(mu);
        closePort_nolock();
    }

    bool isOpen() const {
        std::lock_guard<std::mutex> lk(mu);
        return fd >= 0;
    }

    bool sendByte(uint8_t b) {
        std::lock_guard<std::mutex> lk(mu);
        if (fd < 0) return false;

        const ssize_t n = ::write(fd, &b, 1);
        if (n != 1) {
            qDebug().noquote() << "UART: write failed errno=" << errno;
            return false;
        }
        tcdrain(fd);
        return true;
    }

    bool readFramedPayload(std::vector<uint8_t>& payloadOut, int timeoutMs) {
        payloadOut.clear();

        int localFd;
        {
            std::lock_guard<std::mutex> lk(mu);
            localFd = fd;
        }
        if (localFd < 0) return false;

        bool inFrame = false;
        auto t0 = std::chrono::steady_clock::now();

        while (true) {
            auto now = std::chrono::steady_clock::now();
            int elapsed = int(std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count());
            if (elapsed >= timeoutMs) return false;

            int remaining = timeoutMs - elapsed;
            if (remaining < 0) remaining = 0;

            pollfd pfd{};
            pfd.fd = localFd;
            pfd.events = POLLIN;

            int pr = ::poll(&pfd, 1, remaining);
            if (pr < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (pr == 0) return false;

            uint8_t buf[512];
            ssize_t n = ::read(localFd, buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                return false;
            }
            if (n == 0) continue;

            for (ssize_t i = 0; i < n; ++i) {
                uint8_t c = buf[i];

                if (!inFrame) {
                    if (c == STX) {
                        inFrame = true;
                        payloadOut.clear();
                    }
                    continue;
                }

                if (c == ETX) return true;

                payloadOut.push_back(c);
                if (int(payloadOut.size()) > UART_MAX_PAYLOAD) return false;
            }
        }
    }

    // SBIM (cmd2=01) => 15, sinon => 25
    static int logCodeFromCmdByte(uint8_t cmdByte) {
        uint8_t cmd2 = (cmdByte >> 4) & 0x03;
        return (cmd2 == 0x01) ? 15 : 25;
    }

    void transactByte(uint8_t cmdByte,
                      int timeoutMs,
                      std::function<void(const QString&)> statusCb,
                      std::function<void(bool ok, const std::vector<uint8_t>& payload)> doneCb)
    {
        std::thread([=]{
            bool okAll = true;

            if (!this->isOpen()) {
                if (!const_cast<UartLink*>(this)->openPort())
                    okAll = false;
            }

            QMetaObject::invokeMethod(qApp, [=]{
                if (statusCb) statusCb("Communication en cours...");
            }, Qt::QueuedConnection);

            if (!okAll) {
                QMetaObject::invokeMethod(qApp, [=]{
                    if (statusCb) statusCb("Communication terminée (ERREUR ouverture UART)");
                    if (doneCb) doneCb(false, {});
                }, Qt::QueuedConnection);
                return;
            }

            bool okTx = const_cast<UartLink*>(this)->sendByte(cmdByte);
            if (!okTx) okAll = false;

            std::vector<uint8_t> payload;
            bool okRx = false;
            if (okAll) {
                okRx = const_cast<UartLink*>(this)->readFramedPayload(payload, timeoutMs);
                if (!okRx) okAll = false;
            }

            if (okRx) {
                int logCode = UartLink::logCodeFromCmdByte(cmdByte);

                bool printable = true;
                for (uint8_t c : payload) {
                    if (c == '\r' || c == '\n' || c == '\t') continue;
                    if (c < 32 || c > 126) { printable = false; break; }
                }

                if (printable) {
                    QByteArray ba(reinterpret_cast<const char*>(payload.data()), int(payload.size()));
                    write_log_line(logCode, QString::fromLatin1(ba));
                } else {
                    QString hex;
                    hex.reserve(int(payload.size()) * 3);
                    for (size_t i = 0; i < payload.size(); ++i) {
                        hex += QString("%1").arg(payload[i], 2, 16, QChar('0')).toUpper();
                        if (i + 1 < payload.size()) hex += ' ';
                    }
                    write_log_line(logCode, hex);
                }
            }

            QMetaObject::invokeMethod(qApp, [=]{
                if (statusCb) statusCb(okAll ? "Communication terminée"
                                            : "Communication terminée (ERREUR/TIMEOUT)");
                if (doneCb) doneCb(okAll, payload);
            }, Qt::QueuedConnection);

        }).detach();
    }

private:
    void closePort_nolock() {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    QString device;
    int baudrate = 115200;
    mutable std::mutex mu;
    int fd = -1;
};

static uint8_t makeFrame(uint8_t cmd2, uint8_t param2) {
    cmd2 &= 0x03;
    param2 &= 0x03;
    return uint8_t(0xC0 | (cmd2 << 4) | (param2 << 2));
}

static QString hexByte(uint8_t b) {
    return QString("0x%1").arg(QString::number(b, 16).toUpper().rightJustified(2, '0'));
}

// =========================== GPIO -> Qt Keys (Clavier) ===========================
class GpioKeys : public QObject {
public:
    struct Map { int bcm; int qtKey; };

    GpioKeys(QObject* parent, QWidget* targetWindow)
        : QObject(parent), target(targetWindow) {}

    ~GpioKeys() { stop(); }

    bool start(const std::vector<Map>& mapping, const char* chipName = "/dev/gpiochip4") {
        stop();

        chip = gpiod_chip_open(chipName);
        if (!chip) {
            qDebug().noquote() << "GPIO: impossible d'ouvrir" << chipName;
            return false;
        }

        maps = mapping;
        lines.clear();
        lines.reserve(maps.size());

        for (auto& m : maps) {
            gpiod_line* line = gpiod_chip_get_line(chip, m.bcm);
            if (!line) {
                qDebug().noquote() << "GPIO: ligne introuvable bcm=" << m.bcm;
                stop();
                return false;
            }

            if (gpiod_line_request_both_edges_events(line, "qt-gpio") < 0) {
                qDebug().noquote() << "GPIO: request both edges failed bcm=" << m.bcm;
                stop();
                return false;
            }

            lines.push_back(line);
        }

        running = true;
        worker = std::thread([this]{ loop(); });

        qDebug().noquote() << "GPIO: OK -> lignes actives =" << int(lines.size()) << " sur " << chipName;
        return true;
    }

    void stop() {
        running = false;
        if (worker.joinable()) worker.join();

        for (auto* l : lines) {
            if (l) gpiod_line_release(l);
        }
        lines.clear();

        if (chip) {
            gpiod_chip_close(chip);
            chip = nullptr;
        }
    }

private:
    // Envoie au focus si dispo, sinon fenêtre active
    void sendKey(int key) {
        QWidget* w = QApplication::focusWidget();
        if (!w) w = QApplication::activeWindow();
        if (!w) w = target;
        QCoreApplication::postEvent(w, new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier));
        QCoreApplication::postEvent(w, new QKeyEvent(QEvent::KeyRelease, key, Qt::NoModifier));
    }

    void loop() {
        const auto debounce = std::chrono::milliseconds(40);
        std::vector<std::chrono::steady_clock::time_point> lastFire(lines.size());
        std::vector<bool> locked(lines.size(), false);

        while (running) {
            for (size_t i = 0; i < lines.size(); ++i) {
                if (!running) break;

                timespec timeout{0, 50 * 1000 * 1000};
                int ret = gpiod_line_event_wait(lines[i], &timeout);
                if (ret <= 0) continue;

                gpiod_line_event ev;
                if (gpiod_line_event_read(lines[i], &ev) != 0) continue;

                auto now = std::chrono::steady_clock::now();

                if (ev.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
                    if (locked[i]) continue;
                    if (now - lastFire[i] < debounce) continue;

                    locked[i] = true;
                    lastFire[i] = now;
                    sendKey(maps[i].qtKey);
                } else if (ev.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
                    locked[i] = false;
                }
            }
        }
    }

    QWidget* target = nullptr;
    gpiod_chip* chip = nullptr;

    std::vector<Map> maps;
    std::vector<gpiod_line*> lines;

    std::thread worker;
    std::atomic<bool> running{false};
};

// =========================== INTRO VIDEO ===========================
class VideoIntroPage : public QWidget {
public:
    explicit VideoIntroPage(QStackedWidget* stack, int nextIndex, QWidget* parent=nullptr)
        : QWidget(parent), stack(stack), nextIndex(nextIndex)
    {
        setFocusPolicy(Qt::StrongFocus);

        player = new QMediaPlayer(this);
        video  = new QVideoWidget(this);

        audio = new QAudioOutput(this);
        audio->setVolume(0.0);
        player->setAudioOutput(audio);

        player->setVideoOutput(video);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(0,0,0,0);
        lay->addWidget(video);

        connect(player, &QMediaPlayer::mediaStatusChanged, this,
                [this](QMediaPlayer::MediaStatus st){
            if (st == QMediaPlayer::EndOfMedia) finish();
        });

        connect(player, &QMediaPlayer::errorOccurred, this,
                [this](QMediaPlayer::Error err, const QString &errStr){
            qDebug().noquote() << "QMediaPlayer error =" << int(err) << errStr;
            finish();
        });

        auto *escLocal = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        escLocal->setContext(Qt::WidgetWithChildrenShortcut);
        connect(escLocal, &QShortcut::activated, this, [this]{ finish(); });
    }

    void start() {
        done = false;

        QString path = QDir::homePath() + "/pge/intro_lecteur.mp4";
        qDebug().noquote() << "Intro video =" << path << "exists=" << QFileInfo(path).exists();

        if (!QFileInfo(path).exists()) {
            finish();
            return;
        }

        player->setSource(QUrl::fromLocalFile(path));
        player->play();
    }

protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        video->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    }

private:
    void finish() {
        if (done) return;
        done = true;

        if (player) player->stop();
        if (stack) stack->setCurrentIndex(nextIndex);
    }

    QStackedWidget* stack = nullptr;
    int nextIndex = 1;

    QMediaPlayer* player = nullptr;
    QVideoWidget* video = nullptr;
    QAudioOutput* audio = nullptr;
    bool done = false;
};

// =========================== MENU ===========================
class MenuPage : public QWidget {
public:
    MenuPage(QWidget* parent=nullptr) : QWidget(parent) {
        auto *root = new QVBoxLayout(this);

        auto *t = new QLabel("PGE - Interface Qt");
        auto *h = new QLabel("Choisis une action (↑/↓ + Entrée) :");

        btnWrite = new QPushButton("Lecture de données (UART)");
        btnLogs  = new QPushButton("Logs (lire)");
        btnQuit  = new QPushButton("Quitter");

        root->addWidget(t);
        root->addWidget(h);
        root->addSpacing(10);
        root->addWidget(btnWrite);
        root->addWidget(btnLogs);
        root->addWidget(btnQuit);
        root->addStretch(1);

        currentChoice = 0;
        updateHighlight();
        setFocusPolicy(Qt::StrongFocus);
    }

    void selectNext() { currentChoice = (currentChoice + 1) % 3; updateHighlight(); }
    void selectPrev() { currentChoice = (currentChoice + 2) % 3; updateHighlight(); }

    void activateSelected() {
        if (currentChoice == 0) btnWrite->click();
        else if (currentChoice == 1) btnLogs->click();
        else btnQuit->click();
    }

    QPushButton *btnWrite=nullptr, *btnLogs=nullptr, *btnQuit=nullptr;

private:
    void updateHighlight() {
        auto setBtn = [](QPushButton* b, bool on){
            b->setProperty("sel", on);
            b->style()->unpolish(b);
            b->style()->polish(b);
            b->update();
        };
        setBtn(btnWrite, currentChoice == 0);
        setBtn(btnLogs,  currentChoice == 1);
        setBtn(btnQuit,  currentChoice == 2);
    }

    int currentChoice = 0;
};

// =========================== UART MENU PAGE ===========================
class UartCommandPage : public QWidget {
public:
    explicit UartCommandPage(QStackedWidget* stack, UartLink* uart, int menuIndex, QWidget* parent=nullptr)
        : QWidget(parent), stack(stack), uart(uart), menuIndex(menuIndex)
    {
        auto *root = new QVBoxLayout(this);

        title  = new QLabel("PAGE 2 - COMMANDES UART");
        help   = new QLabel("↑/↓ choisir | Entrée valider | ESC menu");
        status = new QLabel("UART: ...");

        btnSBIB = new QPushButton("Lire Balise SBIB");
        btnSBIM = new QPushButton("Sortie SBIM");
        btnUCS  = new QPushButton("Menu UCS");
        btnTEST = new QPushButton("Test passage train");
        btnBack = new QPushButton("Retour");

        root->addWidget(title);
        root->addWidget(help);
        root->addSpacing(10);
        root->addWidget(btnSBIB);
        root->addWidget(btnSBIM);
        root->addWidget(btnUCS);
        root->addWidget(btnTEST);
        root->addSpacing(10);
        root->addWidget(btnBack);
        root->addWidget(status);
        root->addStretch(1);

        for (QPushButton* b : {btnSBIB, btnSBIM, btnUCS, btnTEST, btnBack}) {
            b->setFocusPolicy(Qt::NoFocus);
        }
        setFocusPolicy(Qt::StrongFocus);

        currentChoice = 0;
        updateHighlight();

        connect(btnSBIB, &QPushButton::clicked, this, [this]{ sendCmdAndWait(0, 0, "SBIB"); });
        connect(btnTEST, &QPushButton::clicked, this, [this]{ sendCmdAndWait(3, 0, "TEST"); });

        connect(btnBack, &QPushButton::clicked, this, [this]{
            if (this->stack) this->stack->setCurrentIndex(this->menuIndex);
        });

        connect(btnSBIM, &QPushButton::clicked, this, [this]{
            if (this->stack) this->stack->setCurrentIndex(this->sbimIndex);
        });

        connect(btnUCS, &QPushButton::clicked, this, [this]{
            if (this->stack) this->stack->setCurrentIndex(this->ucsIndex);
        });

        auto *kEnter = new QShortcut(QKeySequence(Qt::Key_Return), this);
        kEnter->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kEnter, &QShortcut::activated, this, [this]{ activateSelected(); });

        auto *kEnter2 = new QShortcut(QKeySequence(Qt::Key_Enter), this);
        kEnter2->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kEnter2, &QShortcut::activated, this, [this]{ activateSelected(); });

        auto *kUp = new QShortcut(QKeySequence(Qt::Key_Up), this);
        kUp->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kUp, &QShortcut::activated, this, [this]{ selectPrev(); });

        auto *kDown = new QShortcut(QKeySequence(Qt::Key_Down), this);
        kDown->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kDown, &QShortcut::activated, this, [this]{ selectNext(); });

        refreshUartState();
    }

    void setSbimIndex(int idx) { sbimIndex = idx; }
    void setUcsIndex(int idx) { ucsIndex = idx; }
    void focusMe() { this->setFocus(); }

    void refreshUartState() {
        if (!uart) return;
        if (!uart->isOpen()) {
            bool ok = uart->openPort();
            status->setText(ok ? "UART: pret (/dev/serial0, 115200)" : "UART: ERREUR ouverture /dev/serial0");
        } else {
            status->setText("UART: pret (/dev/serial0, 115200)");
        }
    }

protected:

    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape) {
            if (stack) stack->setCurrentIndex(menuIndex);
            e->accept();
            return;
        }
        QWidget::keyPressEvent(e);
    }

private:
    void selectNext() { currentChoice = (currentChoice + 1) % 5; updateHighlight(); }
    void selectPrev() { currentChoice = (currentChoice + 4) % 5; updateHighlight(); }

    void activateSelected() {
        switch (currentChoice) {
            case 0: btnSBIB->click(); break;
            case 1: btnSBIM->click(); break;
            case 2: btnUCS->click();  break;
            case 3: btnTEST->click(); break;
            case 4: btnBack->click(); break;
            default: break;
        }
    }

    void updateHighlight() {
        auto setBtn = [](QPushButton* b, bool on){
            b->setProperty("sel", on);
            b->style()->unpolish(b);
            b->style()->polish(b);
            b->update();
        };
        setBtn(btnSBIB, currentChoice == 0);
        setBtn(btnSBIM, currentChoice == 1);
        setBtn(btnUCS,  currentChoice == 2);
        setBtn(btnTEST, currentChoice == 3);
        setBtn(btnBack, currentChoice == 4);
    }

    void sendCmdAndWait(uint8_t cmd2, uint8_t param2, const QString& label) {
        if (!uart) return;

        uint8_t cmdByte = makeFrame(cmd2, param2);

        QString txMsg = QString("TX %1 (cmd=%2 param=%3) -> %4")
                            .arg(label).arg(cmd2).arg(param2).arg(hexByte(cmdByte));

        uart->transactByte(
            cmdByte,
            UART_RESPONSE_TIMEOUT_MS,
            [this, txMsg](const QString& s){
                this->status->setText(s + " | " + txMsg);
            },
            [this](bool ok, const std::vector<uint8_t>& payload){
                Q_UNUSED(payload);
                this->status->setText(ok ? "Communication terminée" : "Communication terminée (ERREUR/TIMEOUT)");
            }
        );
    }

    QStackedWidget* stack = nullptr;
    UartLink* uart = nullptr;

    int menuIndex = 1;
    int sbimIndex = 3;
    int ucsIndex  = 4;

    QLabel *title=nullptr, *help=nullptr, *status=nullptr;
    QPushButton *btnSBIB=nullptr, *btnSBIM=nullptr, *btnUCS=nullptr, *btnTEST=nullptr, *btnBack=nullptr;

    int currentChoice = 0;
};

// =========================== SBIM SELECT PAGE ===========================
class SbimSelectPage : public QWidget {
public:
    explicit SbimSelectPage(QStackedWidget* stack, UartLink* uart, int backIndex, QWidget* parent=nullptr)
        : QWidget(parent), stack(stack), uart(uart), backIndex(backIndex)
    {
        auto *root = new QVBoxLayout(this);

        title  = new QLabel("PAGE 2B - SORTIE SBIM");
        help   = new QLabel("↑/↓ choisir | Entrée envoyer | ESC retour");
        status = new QLabel("");

        b1 = new QPushButton("1");
        b2 = new QPushButton("2");
        b3 = new QPushButton("3");
        b4 = new QPushButton("4");
        back = new QPushButton("Retour");

        root->addWidget(title);
        root->addWidget(help);
        root->addSpacing(10);
        root->addWidget(b1);
        root->addWidget(b2);
        root->addWidget(b3);
        root->addWidget(b4);
        root->addSpacing(10);
        root->addWidget(back);
        root->addWidget(status);
        root->addStretch(1);

        for (QPushButton* b : {b1, b2, b3, b4, back}) {
            b->setFocusPolicy(Qt::NoFocus);
        }
        setFocusPolicy(Qt::StrongFocus);

        currentChoice = 0;
        updateHighlight();

        connect(b1, &QPushButton::clicked, this, [this]{ sendSbimAndWait(0, "SBIM-1"); });
        connect(b2, &QPushButton::clicked, this, [this]{ sendSbimAndWait(1, "SBIM-2"); });
        connect(b3, &QPushButton::clicked, this, [this]{ sendSbimAndWait(2, "SBIM-3"); });
        connect(b4, &QPushButton::clicked, this, [this]{ sendSbimAndWait(3, "SBIM-4"); });

        connect(back, &QPushButton::clicked, this, [this]{
            if (this->stack) this->stack->setCurrentIndex(this->backIndex);
        });

        auto *kEnter = new QShortcut(QKeySequence(Qt::Key_Return), this);
        kEnter->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kEnter, &QShortcut::activated, this, [this]{ activateSelected(); });

        auto *kEnter2 = new QShortcut(QKeySequence(Qt::Key_Enter), this);
        kEnter2->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kEnter2, &QShortcut::activated, this, [this]{ activateSelected(); });

        auto *kUp = new QShortcut(QKeySequence(Qt::Key_Up), this);
        kUp->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kUp, &QShortcut::activated, this, [this]{ selectPrev(); });

        auto *kDown = new QShortcut(QKeySequence(Qt::Key_Down), this);
        kDown->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kDown, &QShortcut::activated, this, [this]{ selectNext(); });
    }

    void focusMe() { this->setFocus(); }

protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape) {
            if (stack) stack->setCurrentIndex(backIndex);
            e->accept();
            return;
        }
        QWidget::keyPressEvent(e);
    }

private:
    void selectNext() { currentChoice = (currentChoice + 1) % 5; updateHighlight(); }
    void selectPrev() { currentChoice = (currentChoice + 4) % 5; updateHighlight(); }

    void activateSelected() {
        switch (currentChoice) {
            case 0: b1->click(); break;
            case 1: b2->click(); break;
            case 2: b3->click(); break;
            case 3: b4->click(); break;
            case 4: back->click(); break;
            default: break;
        }
    }

    void updateHighlight() {
        auto setBtn = [](QPushButton* b, bool on){
            b->setProperty("sel", on);
            b->style()->unpolish(b);
            b->style()->polish(b);
            b->update();
        };
        setBtn(b1, currentChoice == 0);
        setBtn(b2, currentChoice == 1);
        setBtn(b3, currentChoice == 2);
        setBtn(b4, currentChoice == 3);
        setBtn(back, currentChoice == 4);
    }

    void sendSbimAndWait(uint8_t param2, const QString& label) {
        if (!uart) return;

        uint8_t cmd2 = 1; // SBIM
        uint8_t cmdByte = makeFrame(cmd2, param2);

        QString txMsg = QString("TX %1 (cmd=%2 param=%3) -> %4")
                            .arg(label).arg(cmd2).arg(param2).arg(hexByte(cmdByte));

        uart->transactByte(
            cmdByte,
            UART_RESPONSE_TIMEOUT_MS,
            [this, txMsg](const QString& s){
                this->status->setText(s + " | " + txMsg);
            },
            [this](bool ok, const std::vector<uint8_t>& payload){
                if (!ok) {
                    this->status->setText("Communication terminée (ERREUR/TIMEOUT)");
                } else {
                    this->status->setText(QString("Communication terminée (payload %1 octets)")
                                          .arg(payload.size()));
                }
            }
        );
    }

    QStackedWidget* stack = nullptr;
    UartLink* uart = nullptr;
    int backIndex = 0;

    QLabel *title=nullptr, *help=nullptr, *status=nullptr;
    QPushButton *b1=nullptr, *b2=nullptr, *b3=nullptr, *b4=nullptr, *back=nullptr;

    int currentChoice = 0;
};

// =========================== UCS SELECT PAGE ===========================
class UcsSelectPage : public QWidget {
public:
    explicit UcsSelectPage(QStackedWidget* stack, UartLink* uart, int backIndex, QWidget* parent=nullptr)
        : QWidget(parent), stack(stack), uart(uart), backIndex(backIndex)
    {
        auto *root = new QVBoxLayout(this);

        title  = new QLabel("PAGE 2C - MENU UCS");
        help   = new QLabel("↑/↓ choisir | Entrée envoyer | ESC retour");
        status = new QLabel("");

        o1 = new QPushButton("Affichage de la configuration");
        o2 = new QPushButton("Lecture incident");
        o3 = new QPushButton("Etat des entrées");
        o4 = new QPushButton("Effacement mémoire");
        back = new QPushButton("Retour");

        root->addWidget(title);
        root->addWidget(help);
        root->addSpacing(10);
        root->addWidget(o1);
        root->addWidget(o2);
        root->addWidget(o3);
        root->addWidget(o4);
        root->addSpacing(10);
        root->addWidget(back);
        root->addWidget(status);
        root->addStretch(1);

        for (QPushButton* b : {o1, o2, o3, o4, back}) {
            b->setFocusPolicy(Qt::NoFocus);
        }
        setFocusPolicy(Qt::StrongFocus);

        currentChoice = 0;
        updateHighlight();

        // UCS cmd2 = 2, param2 = 0..3 => 1110xx00
        connect(o1, &QPushButton::clicked, this, [this]{ sendUcsAndWait(0, "UCS-CONFIG"); });
        connect(o2, &QPushButton::clicked, this, [this]{ sendUcsAndWait(1, "UCS-INCIDENT"); });
        connect(o3, &QPushButton::clicked, this, [this]{ sendUcsAndWait(2, "UCS-ETAT"); });
        connect(o4, &QPushButton::clicked, this, [this]{ sendUcsAndWait(3, "UCS-ERASE"); });

        connect(back, &QPushButton::clicked, this, [this]{
            if (this->stack) this->stack->setCurrentIndex(this->backIndex);
        });

        auto *kEnter = new QShortcut(QKeySequence(Qt::Key_Return), this);
        kEnter->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kEnter, &QShortcut::activated, this, [this]{ activateSelected(); });

        auto *kEnter2 = new QShortcut(QKeySequence(Qt::Key_Enter), this);
        kEnter2->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kEnter2, &QShortcut::activated, this, [this]{ activateSelected(); });

        auto *kUp = new QShortcut(QKeySequence(Qt::Key_Up), this);
        kUp->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kUp, &QShortcut::activated, this, [this]{ selectPrev(); });

        auto *kDown = new QShortcut(QKeySequence(Qt::Key_Down), this);
        kDown->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kDown, &QShortcut::activated, this, [this]{ selectNext(); });
    }

    void focusMe() { this->setFocus(); }

protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape) {
            if (stack) stack->setCurrentIndex(backIndex);
            e->accept();
            return;
        }
        QWidget::keyPressEvent(e);
    }

private:
    void selectNext() { currentChoice = (currentChoice + 1) % 5; updateHighlight(); }
    void selectPrev() { currentChoice = (currentChoice + 4) % 5; updateHighlight(); }

    void activateSelected() {
        switch (currentChoice) {
            case 0: o1->click(); break;
            case 1: o2->click(); break;
            case 2: o3->click(); break;
            case 3: o4->click(); break;
            case 4: back->click(); break;
            default: break;
        }
    }

    void updateHighlight() {
        auto setBtn = [](QPushButton* b, bool on){
            b->setProperty("sel", on);
            b->style()->unpolish(b);
            b->style()->polish(b);
            b->update();
        };
        setBtn(o1, currentChoice == 0);
        setBtn(o2, currentChoice == 1);
        setBtn(o3, currentChoice == 2);
        setBtn(o4, currentChoice == 3);
        setBtn(back, currentChoice == 4);
    }

    void sendUcsAndWait(uint8_t param2, const QString& label) {
        if (!uart) return;

        uint8_t cmd2 = 2; // UCS
        uint8_t cmdByte = makeFrame(cmd2, param2);

        QString txMsg = QString("TX %1 (cmd=%2 param=%3) -> %4")
                            .arg(label).arg(cmd2).arg(param2).arg(hexByte(cmdByte));

        uart->transactByte(
            cmdByte,
            UART_RESPONSE_TIMEOUT_MS,
            [this, txMsg](const QString& s){
                this->status->setText(s + " | " + txMsg);
            },
            [this](bool ok, const std::vector<uint8_t>& payload){
                if (!ok) {
                    this->status->setText("Communication terminée (ERREUR/TIMEOUT)");
                } else {
                    this->status->setText(QString("Communication terminée (payload %1 octets)")
                                          .arg(payload.size()));
                }
            }
        );
    }

    QStackedWidget* stack = nullptr;
    UartLink* uart = nullptr;
    int backIndex = 0;

    QLabel *title=nullptr, *help=nullptr, *status=nullptr;
    QPushButton *o1=nullptr, *o2=nullptr, *o3=nullptr, *o4=nullptr, *back=nullptr;

    int currentChoice = 0;
};

// =========================== LOGS PAGE ===========================
class LogsListWidget : public QListWidget {
public:
    using QListWidget::QListWidget;
    std::function<void()> onLeft;
    std::function<void()> onRight;
    std::function<void()> onR;
    std::function<void()> onEnter;
protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Left)   { if (onLeft)  onLeft();  e->accept(); return; }
        if (e->key() == Qt::Key_Right)  { if (onRight) onRight(); e->accept(); return; }
        if (e->key() == Qt::Key_R)      { if (onR)     onR();     e->accept(); return; }
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            if (onEnter) onEnter();
            e->accept();
            return;
        }
        QListWidget::keyPressEvent(e);
    }
};

class LogsPage : public QWidget {
public:
    explicit LogsPage(QStackedWidget* stack, int menuIndex, QWidget* parent=nullptr)
        : QWidget(parent), stack(stack), menuIndex(menuIndex)
    {
        auto *root = new QVBoxLayout(this);

        title = new QLabel("PAGE 3 - LOGS");
        help  = new QLabel("←/→ pages | R rafraichir | A=tous | 1=15 | 2=25 | O=autres | ESC menu");

        list  = new LogsListWidget;

        auto *bar = new QHBoxLayout;
        btnBack = new QPushButton("Retour");
        btnPrev = new QPushButton("◀");
        btnNext = new QPushButton("▶");
        btnRefresh = new QPushButton("Rafraichir");
        pageInfo = new QLabel;

        bar->addWidget(btnBack);
        bar->addSpacing(10);
        bar->addWidget(btnPrev);
        bar->addWidget(btnNext);
        bar->addWidget(btnRefresh);
        bar->addStretch(1);
        bar->addWidget(pageInfo);

        auto *filterBar = new QHBoxLayout;
        filterBar->addWidget(new QLabel("Filtre :"));
        btnAll = new QPushButton("Tous");
        btn15  = new QPushButton("15");
        btn25  = new QPushButton("25");
        btnOther = new QPushButton("Autres");
        filterInfo = new QLabel;

        filterBar->addWidget(btnAll);
        filterBar->addWidget(btn15);
        filterBar->addWidget(btn25);
        filterBar->addWidget(btnOther);
        filterBar->addStretch(1);
        filterBar->addWidget(filterInfo);

        root->addWidget(title);
        root->addWidget(help);
        root->addLayout(bar);
        root->addLayout(filterBar);
        root->addWidget(list, 1);

        connect(btnRefresh, &QPushButton::clicked, this, [this]{ reload(); });
        connect(btnPrev, &QPushButton::clicked, this, [this]{ prevPage(); });
        connect(btnNext, &QPushButton::clicked, this, [this]{ nextPage(); });

        connect(btnBack, &QPushButton::clicked, this, [this]{
            if (this->stack) this->stack->setCurrentIndex(this->menuIndex);
        });

        list->onLeft  = [this]{ btnPrev->click(); };
        list->onRight = [this]{ btnNext->click(); };
        list->onR     = [this]{ btnRefresh->click(); };
        list->onEnter = [this]{ cyclefilter(); };

        connect(btnAll,   &QPushButton::clicked, this, [this]{ setFilter(LogFilter::All); });
        connect(btn15,    &QPushButton::clicked, this, [this]{ setFilter(LogFilter::Only15); });
        connect(btn25,    &QPushButton::clicked, this, [this]{ setFilter(LogFilter::Only25); });
        connect(btnOther, &QPushButton::clicked, this, [this]{ setFilter(LogFilter::Other); });

        auto *kAll = new QShortcut(QKeySequence(Qt::Key_A), this);
        kAll->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kAll, &QShortcut::activated, this, [this]{ setFilter(LogFilter::All); });

        auto *k15 = new QShortcut(QKeySequence(Qt::Key_1), this);
        k15->setContext(Qt::WidgetWithChildrenShortcut);
        connect(k15, &QShortcut::activated, this, [this]{ setFilter(LogFilter::Only15); });

        auto *k25 = new QShortcut(QKeySequence(Qt::Key_2), this);
        k25->setContext(Qt::WidgetWithChildrenShortcut);
        connect(k25, &QShortcut::activated, this, [this]{ setFilter(LogFilter::Only25); });

        auto *kOther = new QShortcut(QKeySequence(Qt::Key_O), this);
        kOther->setContext(Qt::WidgetWithChildrenShortcut);
        connect(kOther, &QShortcut::activated, this, [this]{ setFilter(LogFilter::Other); });

        pageSize = 18;
        filter = LogFilter::All;
        reload();
    }

    void focusList() { list->setFocus(); }

    void reload() {
        const int maxKeep = MAX_PAGES * pageSize;
        rawLogs = load_last_logs_limited(maxKeep);
        applyFilter();
        page = 0;
        render();
    }

    void nextPage() { if (page + 1 < totalPages) { page++; render(); } }
    void prevPage() { if (page > 0) { page--; render(); } }

private:
    void setFilter(LogFilter f) {
        filter = f;
        applyFilter();
        page = 0;
        render();
    }

    bool matchFilter(const QString& line) const {
        const bool has15 = line.contains("(15)");
        const bool has25 = line.contains("(25)");

        if (filter == LogFilter::All) return true;
        if (filter == LogFilter::Only15) return has15;
        if (filter == LogFilter::Only25) return has25;
        if (filter == LogFilter::Other) return (!has15 && !has25);
        return true;
    }

    void applyFilter() {
        filteredLogs.clear();
        for (const QString& l : rawLogs) {
            if (matchFilter(l)) filteredLogs.push_back(l);
        }

        int count = filteredLogs.size();
        totalPages = (count + pageSize - 1) / pageSize;
        if (totalPages < 1) totalPages = 1;
        if (totalPages > MAX_PAGES) totalPages = MAX_PAGES;

        updateFilterUi();
    }

    void updateFilterUi() {
        auto setBtn = [](QPushButton* b, bool on){
            b->setProperty("sel", on);
            b->style()->unpolish(b);
            b->style()->polish(b);
            b->update();
        };

        setBtn(btnAll,   filter == LogFilter::All);
        setBtn(btn15,    filter == LogFilter::Only15);
        setBtn(btn25,    filter == LogFilter::Only25);
        setBtn(btnOther, filter == LogFilter::Other);

        QString txt = "Tous";
        if (filter == LogFilter::Only15) txt = "15";
        if (filter == LogFilter::Only25) txt = "25";
        if (filter == LogFilter::Other)  txt = "Autres";

        filterInfo->setText(QString("filtre: %1 | lignes: %2")
                            .arg(txt)
                            .arg(filteredLogs.size()));
    }

    void render() {
        list->clear();

        const int count = filteredLogs.size();
        int startRank = page * pageSize;

        for (int i = 0; i < pageSize; i++) {
            int recentRank = startRank + i;
            int idx = (count - 1) - recentRank;
            if (idx < 0) break;
            list->addItem(filteredLogs[idx]);
        }

        pageInfo->setText(QString("page %1/%2").arg(page + 1).arg(totalPages));
        btnPrev->setEnabled(page > 0);
        btnNext->setEnabled(page + 1 < totalPages);
    }

    void cyclefilter(){
        filterCycle = (filterCycle + 1) % 4;
        if (filterCycle == 0) setFilter(LogFilter::All);
        if (filterCycle == 1) setFilter(LogFilter::Only15);
        if (filterCycle == 2) setFilter(LogFilter::Only25);
        if (filterCycle == 3) setFilter(LogFilter::Other);
    }

    QStackedWidget* stack = nullptr;
    int menuIndex = 1;

    QLabel *title=nullptr, *help=nullptr, *pageInfo=nullptr;
    LogsListWidget *list=nullptr;

    QPushButton *btnBack=nullptr;
    QPushButton *btnPrev=nullptr;
    QPushButton *btnNext=nullptr;
    QPushButton *btnRefresh=nullptr;

    QPushButton *btnAll=nullptr;
    QPushButton *btn15=nullptr;
    QPushButton *btn25=nullptr;
    QPushButton *btnOther=nullptr;
    QLabel *filterInfo=nullptr;

    QStringList rawLogs;
    QStringList filteredLogs;

    LogFilter filter = LogFilter::All;
    int filterCycle = 0;
    int page = 0;
    int pageSize = 18;
    int totalPages = 1;
};

// =========================== MAIN ===========================
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setStyleSheet(R"(
        QMainWindow, QWidget { background: #0b2a6b; color: white; }

        QPushButton {
            background: #ffffff;
            color: #000000;
            padding: 8px 10px;
            border-radius: 6px;
        }
        QPushButton:hover { background: #e6e6e6; }
        QPushButton[sel="true"] { border: 3px solid #ffd54a; }

        QLabel { font-size: 14px; }
        QListWidget {
            background: rgba(255,255,255,0.08);
            border: 1px solid rgba(255,255,255,0.2);
        }
    )");

    QFile touch(logPath());
    if (!touch.exists()) {
        touch.open(QIODevice::WriteOnly | QIODevice::Append);
        touch.close();
    }

    QMainWindow win;
    win.setWindowTitle("PGE - Qt");
    win.resize(900, 520);

    auto *stack = new QStackedWidget;

    UartLink uart("/dev/serial0", 115200);

    // 0 = intro
    // 1 = menu
    // 2 = uartCmd
    // 3 = sbimSel
    // 4 = ucsSel  
    // 5 = logs
    auto *intro   = new VideoIntroPage(stack, 1);
    auto *menu    = new MenuPage;
    auto *uartCmd = new UartCommandPage(stack, &uart, 1);
    auto *sbimSel = new SbimSelectPage(stack, &uart, 2);
    auto *ucsSel  = new UcsSelectPage(stack, &uart, 2);
    auto *logs    = new LogsPage(stack, 1);

    stack->addWidget(intro);   // 0
    stack->addWidget(menu);    // 1
    stack->addWidget(uartCmd); // 2
    stack->addWidget(sbimSel); // 3
    stack->addWidget(ucsSel);  // 4
    stack->addWidget(logs);    // 5

    uartCmd->setSbimIndex(3);
    uartCmd->setUcsIndex(4);

    win.setCentralWidget(stack);
    win.show();
    win.raise();
    win.activateWindow();

    // GPIO
    auto *gpio = new GpioKeys(&app, &win);
    std::vector<GpioKeys::Map> mapping = {
        {17, Qt::Key_Up},
        {27, Qt::Key_Down},
        {23, Qt::Key_Left},
        {24, Qt::Key_Right},
        {22, Qt::Key_Return},
        {25, Qt::Key_Escape},
    };
    if (!gpio->start(mapping, "/dev/gpiochip4")) {
        qDebug().noquote() << "GPIO non demarre ! Verifie /dev/gpiochip4 et cablage.";
    }

    // Navigation depuis menu
    QObject::connect(menu->btnWrite, &QPushButton::clicked, [&]{
        uartCmd->refreshUartState();
        stack->setCurrentIndex(2);
        uartCmd->focusMe();
    });

    QObject::connect(menu->btnLogs, &QPushButton::clicked, [&]{
        logs->reload();
        stack->setCurrentIndex(5);
        logs->focusList();
    });

    QObject::connect(menu->btnQuit, &QPushButton::clicked, [&]{
        app.quit();
    });

    // Shortcuts menu
    auto *mUp = new QShortcut(QKeySequence(Qt::Key_Up), menu);
    mUp->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(mUp, &QShortcut::activated, menu, [menu]{ menu->selectPrev(); });

    auto *mDown = new QShortcut(QKeySequence(Qt::Key_Down), menu);
    mDown->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(mDown, &QShortcut::activated, menu, [menu]{ menu->selectNext(); });

    auto *mEnter = new QShortcut(QKeySequence(Qt::Key_Return), menu);
    mEnter->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(mEnter, &QShortcut::activated, menu, [menu]{ menu->activateSelected(); });

    auto *mEnter2 = new QShortcut(QKeySequence(Qt::Key_Enter), menu);
    mEnter2->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(mEnter2, &QShortcut::activated, menu, [menu]{ menu->activateSelected(); });

    // ESC global (on garde)
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), &win);
    esc->setContext(Qt::ApplicationShortcut);
    QObject::connect(esc, &QShortcut::activated, [&]{
        int idx = stack->currentIndex();
        if (idx == 0) {
            stack->setCurrentIndex(1);
            menu->setFocus();
        } else if (idx == 2 || idx == 5) {
            stack->setCurrentIndex(1);
            menu->setFocus();
        } else if (idx == 3 || idx == 4) {
            stack->setCurrentIndex(2);
            uartCmd->focusMe();
        } else {
            menu->setFocus();
        }
    });

    qDebug().noquote() << "HOME =" << QDir::homePath();
    qDebug().noquote() << "LOG  =" << logPath();

    stack->setCurrentIndex(0);
    intro->start();

    return app.exec();
}
