#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"

class TachTests : public QObject {
    Q_OBJECT

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    // Builds a card tree that looks like the real /sys/class/drm/card1/device.
    static QString fakeCard(const QString &root)
    {
        const QString card = root + "/card1/device";
        write(card + "/vendor", "0x1002\n");
        write(card + "/device", "0x744c\n");
        write(card + "/mem_info_vram_total", "21458059264\n");
        write(card + "/mem_info_vram_used", "2009632768\n");
        write(card + "/gpu_busy_percent", "68\n");
        write(card + "/mem_busy_percent", "34\n");
        write(card + "/pp_dpm_sclk", "0: 500Mhz\n1: 2075Mhz *\n");
        write(card + "/pp_dpm_mclk", "0: 96Mhz\n1: 1249Mhz *\n");
        write(card + "/current_link_speed", "16.0 GT/s PCIe\n");
        write(card + "/current_link_width", "16\n");
        write(card + "/power_dpm_force_performance_level", "auto\n");
        write(card + "/hwmon/hwmon3/temp1_input", "52000\n");
        write(card + "/hwmon/hwmon3/temp2_input", "59000\n");
        write(card + "/hwmon/hwmon3/temp3_input", "68000\n");
        write(card + "/hwmon/hwmon3/power1_average", "74000000\n");
        write(card + "/hwmon/hwmon3/fan1_input", "0\n");
        write(card + "/hwmon/hwmon3/fan1_max", "3600\n");
        return card;
    }

private slots:
    void parsesFdinfoSizes()
    {
        QCOMPARE(Backend::parseSize("2 MiB"), 2LL * 1024 * 1024);
        QCOMPARE(Backend::parseSize("1 GiB"), 1024LL * 1024 * 1024);
        QCOMPARE(Backend::parseSize("512 KiB"), 512LL * 1024);
        QCOMPARE(Backend::parseSize("0"), 0LL);
        QCOMPARE(Backend::parseSize("nonsense"), 0LL);
    }

    void readsTheActiveClock()
    {
        QCOMPARE(Backend::currentClockMhz("0: 500Mhz\n1: 2075Mhz *\n2: 2400Mhz\n"), 2075);
        QCOMPARE(Backend::currentClockMhz("0: 96Mhz\n1: 1249Mhz *"), 1249);
        // No asterisk at all: fall back to the last listed state rather than 0.
        QCOMPARE(Backend::currentClockMhz("0: 500Mhz\n1: 800Mhz"), 800);
        QCOMPARE(Backend::currentClockMhz(""), 0);
    }

    void sumsVramOnlyForOurCard()
    {
        const QString ourCard = "drm-pdev:\t0000:03:00.0\ndrm-total-vram:\t1 GiB\n";
        const QString otherCard = "drm-pdev:\t0000:7a:00.0\ndrm-total-vram:\t512 MiB\n";
        QCOMPARE(Backend::vramForProcess(ourCard, "0000:03:00.0"), 1024LL * 1024 * 1024);
        QCOMPARE(Backend::vramForProcess(otherCard, "0000:03:00.0"), 0LL);
        QCOMPARE(Backend::vramForProcess("drm-total-gtt:\t2 MiB\n", "0000:03:00.0"), 0LL);
    }

    void namesKnownCards()
    {
        QCOMPARE(Backend::cardNameForDeviceId(0x744c), QString("Radeon RX 7900 XT/XTX"));
        QCOMPARE(Backend::cardNameForDeviceId(0x164e), QString("Radeon 610M"));
        QVERIFY(Backend::cardNameForDeviceId(0xdead).contains("dead"));
    }

    void readsAFakeCard()
    {
        QTemporaryDir dir;
        fakeCard(dir.path());

        Backend backend(dir.path());
        QVERIFY(backend.available());
        QCOMPARE(backend.cardName(), QString("Radeon RX 7900 XT/XTX"));
        QCOMPARE(backend.utilisation(), 68.0);
        QCOMPARE(backend.memUtilisation(), 34.0);
        QCOMPARE(backend.edgeTemp(), 52);
        QCOMPARE(backend.junctionTemp(), 59);
        QCOMPARE(backend.memTemp(), 68);
        QCOMPARE(backend.power(), 74.0);
        QCOMPARE(backend.fanMax(), 3600);
        QCOMPARE(backend.coreClock(), 2075);
        QCOMPARE(backend.memClock(), 1249);
        QCOMPARE(backend.pcie(), QString("16.0 GT/s PCIe x16"));
        QCOMPARE(backend.perfLevel(), QString("auto"));
        QVERIFY(backend.canControl());
        QVERIFY(backend.vramFraction() > 0.09 && backend.vramFraction() < 0.10);
    }

    void ignoresOtherVendors()
    {
        QTemporaryDir dir;
        write(dir.path() + "/card0/device/vendor", "0x10de\n");
        write(dir.path() + "/card0/device/mem_info_vram_total", "8000000000\n");

        Backend backend(dir.path());
        QVERIFY(!backend.available());
        QVERIFY(!backend.error().isEmpty());
    }

    void prefersTheCardWithMoreVram()
    {
        QTemporaryDir dir;
        write(dir.path() + "/card0/device/vendor", "0x1002\n");
        write(dir.path() + "/card0/device/device", "0x164e\n");
        write(dir.path() + "/card0/device/mem_info_vram_total", "536870912\n");
        fakeCard(dir.path());

        Backend backend(dir.path());
        QCOMPARE(backend.cardName(), QString("Radeon RX 7900 XT/XTX"));
    }

    void historyIsBounded()
    {
        QTemporaryDir dir;
        fakeCard(dir.path());

        Backend backend(dir.path());
        for (int i = 0; i < Backend::HistoryLength + 25; ++i)
            backend.poll();

        QCOMPARE(backend.utilHistory().size(), Backend::HistoryLength);
    }

    void reportsMissingControlFile()
    {
        QTemporaryDir dir;
        const QString card = fakeCard(dir.path());
        QFile::remove(card + "/power_dpm_force_performance_level");

        Backend backend(dir.path());
        QVERIFY(backend.available());
        QVERIFY(!backend.canControl());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#82FB9C\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#82FB9C"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themePicksReadableForeground()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme dark(dir.path());
        QCOMPARE(dark.accentForeground(), QString("white"));
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Power").isNull());
    }

    void translatorReadsTheBulgarianCatalog()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "bg\n");
        write(dir.path() + "/bg.json", R"({"Power": "Мощност", "Fan": "Вентилатор"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("bg"));
        QCOMPARE(translator.translate("Main", "Power"), QString("Мощност"));
        QCOMPARE(translator.translate("Main", "Fan"), QString("Вентилатор"));
        // Untranslated strings stay English rather than going blank.
        QVERIFY(translator.translate("Main", "VRAM").isNull());
    }
};

QTEST_MAIN(TachTests)
#include "tach_tests.moc"
