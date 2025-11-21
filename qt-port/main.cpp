#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QRegularExpression>
#include "imagewidget.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <tuple>
#include <cmath>

struct ImageToLoad {
    QString path;
    int x;
    int y;
};

static QStringList scanForFiles(const QDir& dir, const QStringList& filters, bool recursive) {
    QStringList result;
    for (auto& fn : dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot)) {
        result << dir.absoluteFilePath(fn);
    }
    if (recursive) {
        for (auto& subdir : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            result << scanForFiles(subdir.absoluteDir(), filters, true);
        }
    }
    return result;
}

float peekaboo(std::random_device &randomizer, const std::vector<float> &pos, std::vector<float> &weight, float currentLength)
{
    float window = pos.back();

    if (currentLength >= window) {
        return 0.f;
    }

    float halfSize = currentLength / 2;

    int startIndex = 0;
    for (; startIndex < int(pos.size()); ++startIndex) {
        if (pos[startIndex] >= halfSize) {
            --startIndex;
            break;
        }
    }

    int endIndex = pos.size() - 1;
    for (; endIndex >= 0; --endIndex) {
        if (window - pos[endIndex] >= halfSize) {
            endIndex += 2;
            break;
        }
    }

    if (endIndex >= 0 && startIndex < endIndex) {
        float center = static_cast<float>(std::piecewise_linear_distribution<>(pos.begin() + startIndex, pos.begin() + endIndex, weight.begin() + startIndex)(randomizer));
        float left = std::max(0.0f, std::min(center - halfSize, window - currentLength));
        float right = left + currentLength;
        for (size_t i = 0; i < pos.size(); ++i) {
            if (pos[i] >= left && pos[i] <= right) {
                weight[i] = 1.f;
            }
            else {
                weight[i] += 2.f;
            }
        }
        return left;
    }
    return std::uniform_real_distribution<float>(0, window - currentLength)(randomizer);
}

template <typename T>
inline int roundToNearest(T x)
{
    return static_cast<int>(std::floor(x + T(0.5)));
}

template <typename T>
std::tuple<T, T> scaleToFit(T srcWidth, T srcHeight, T destWidth, T destHeight)
{
    if (srcWidth > T(0) && srcHeight > T(0))
    {
        T rw = destHeight * srcWidth / srcHeight;
        if (rw <= destWidth)
        {
            return std::make_tuple(rw, destHeight);
        }
        else
        {
            return std::make_tuple(destWidth, srcHeight / srcWidth * destWidth);
        }
    }
    return std::make_tuple(T(0), T(0));
}

inline std::tuple<int, int> scaleToFit(int srcWidth, int srcHeight, int destWidth, int destHeight)
{
    if (srcWidth > 0 && srcHeight > 0)
    {
        int rw = roundToNearest(destHeight * float(srcWidth) / srcHeight);
        if (rw <= destWidth)
        {
            return std::make_tuple(rw, destHeight);
        }
        else
        {
            return std::make_tuple(destWidth, roundToNearest(srcHeight / (float)srcWidth * destWidth));
        }
    }
    return std::make_tuple(0, 0);
}


class SlideShow : public QObject {
public:
    SlideShow(const QStringList &imageList, int interval, bool borderless, const QRect& geometry):
        _images(imageList),
        _widget(nullptr, borderless ? Qt::FramelessWindowHint : Qt::Widget),
        _interval(interval)
    {
        _widget.setGeometry(geometry);
        _loadTimer = new QTimer(this);
        QObject::connect(_loadTimer, &QTimer::timeout, this, &SlideShow::loadNextImage);
        QObject::connect(&_widget, &ImageWidget::ready, this, &SlideShow::onWidgetReady);
        QObject::connect(&_widget, &ImageWidget::closed, this, &SlideShow::onWidgetClosed);
        QObject::connect(&_widget, &ImageWidget::resized, this, &SlideShow::onWidgetResized);
    }

    void start() {
        _widget.show();
    }

private:
    QStringList _images;
    int _currentIndex = -1;
    ImageWidget _widget;
    int _interval;
    QTimer* _loadTimer;
    std::random_device _randomizer;

    std::vector<float> _weightPosX;
    std::vector<float> _weightValueX;
    std::vector<float> _weightPosY;
    std::vector<float> _weightValueY;

    void loadNextImage() {
        while (true) {
            _currentIndex = (_currentIndex + 1) % _images.size();
            const auto& filePath = _images[_currentIndex];
            QImage image(filePath);
            if (image.isNull()) {
                std::cerr << "Could not load image " << filePath.toStdString() << std::endl;
                if (_currentIndex + 1 >= _images.size()) {
                    break;
                }
                continue;
            }
            auto imgWidth = image.width();
            auto imgHeight = image.height();
            auto maxWidth = _widget.width();
            auto maxHeight = _widget.height();
            if (imgWidth > maxWidth || imgHeight > maxHeight) {
                std::tie(imgWidth, imgHeight) = scaleToFit(imgWidth, imgHeight, maxWidth, maxHeight); // m_renderTarget->GetSize();
            }
            auto newX = peekaboo(_randomizer, _weightPosX, _weightValueX, imgWidth);
            auto newY = peekaboo(_randomizer, _weightPosY, _weightValueY, imgHeight);
            _widget.loadImage(image, roundToNearest(newX), roundToNearest(newY), imgWidth, imgHeight);
            break;
        }
    }

    void onWidgetReady(int w, int h) {
        int numberOfRange = 20;
        initWeightRange(_weightPosX, _weightValueX, numberOfRange, w);
        initWeightRange(_weightPosY, _weightValueY, numberOfRange, h);

        loadNextImage();
        _loadTimer->start(_interval);
    }

    void onWidgetClosed() {
        _loadTimer->stop();
    }

    void onWidgetResized(int w, int h) {
        int numberOfRange = 20;
        initWeightRange(_weightPosX, _weightValueX, numberOfRange, w);
        initWeightRange(_weightPosY, _weightValueY, numberOfRange, h);
    }

    static void initWeightRange(std::vector<float> &weightPos, std::vector<float> &weightValue, unsigned rangeCount, float length)
    {
        weightPos.reserve(rangeCount+1);
        float rangeWidth = length / rangeCount;
        float pos = 0;
        while (weightPos.size() < rangeCount) {
            weightPos.push_back(pos);
            pos = pos + rangeWidth;
        }
        weightPos.push_back(length);
        weightValue.resize(weightPos.size());
        std::fill(weightValue.begin(), weightValue.end(), 1.0f);
    }
};

static void parseGeometry(const QString &geometry, int *x, int *y, int *width, int *height) {
    QRegularExpression regex(R"(^=?(\d+)?(?:[xX](\d+))?([+-]\d+)?([+-]\d+)?$)");
    QRegularExpressionMatch match = regex.match(geometry);

    if (!match.hasMatch()) {
        std::cerr << "Invalid geometry string format: " << geometry.toStdString() << std::endl;
        return;
    }

    QString widthStr = match.captured(1);
    if (!widthStr.isEmpty()) {
        *width = widthStr.toInt();
    }
    QString heightStr = match.captured(2);
    if (!heightStr.isEmpty()) {
        *height = heightStr.toInt();
    }

    // 2. X and Y offsets (must check signs for negative flags)
    QString xOffsetStr = match.captured(3);
    if (!xOffsetStr.isEmpty()) {
        *x = xOffsetStr.toInt();
    }

    QString yOffsetStr = match.captured(4);
    if (!yOffsetStr.isEmpty()) {
        *y = yOffsetStr.toInt();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationVersion("1.0.0");

    QCommandLineOption borderless(QStringList() << "b" << "borderless", "Hide window title and border.");
    QCommandLineOption recursive(QStringList() << "r" << "recursive", "Scan directories recursively to look for images.");
    QCommandLineOption shuffle(QStringList() << "s" << "shuffle", "Shuffle image list once before start the show.");
    QCommandLineOption interval(QStringList() << "t" << "timeout", "Delay (seconds) before loading next image (default: 30).", "seconds", "30");
    QCommandLineOption geometry(QStringList() << "g" << "geometry", "Window geometry (position is ignored on Wayland).", "spec", "1080x768+0+0");
    QCommandLineOption formatfilter(QStringList() << "f" << "format", "List of image formats to scan (default: jpg,jpeg,png,webp).", "extentions", "");

    QCommandLineParser parser;
    parser.setApplicationDescription("Simple Slideshow");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(recursive);
    parser.addOption(shuffle);
    parser.addOption(interval);
    parser.addOption(borderless);
    parser.addOption(geometry);
    parser.addOption(formatfilter);
    parser.process(app);

    QStringList args = parser.positionalArguments();
    QString extToScan = parser.value(formatfilter);
    QStringList filters;
    if (extToScan.isEmpty()) {
        filters = { "*.jpg", ".jpeg", "*.png", "*.webp"};
    } else {
        filters = extToScan.split(',', Qt::SkipEmptyParts);
        for (auto& f : filters) {
            f = "*." + f.trimmed();
        }
    }

    QStringList imageList;
    for (auto& path : args) {
        imageList << scanForFiles(QDir(path), filters, parser.isSet(recursive));
    }

    if (imageList.isEmpty()) {
        std::cout << "No images found" << std::endl;
        return 0;
    }

    if (parser.isSet(shuffle)) {
        std::shuffle(imageList.begin(), imageList.end(), std::random_device());
    }

    int timeout = parser.value(interval).toInt();
    if (timeout <= 0) {
        timeout = 30;
    }

    int x = 0;
    int y = 0;
    int w = 1080;
    int h = 768;
    parseGeometry(parser.value(geometry), &x, &y, &w, &h);

    SlideShow ss(imageList, timeout * 1000, parser.isSet(borderless), QRect(x, y, w, h));
    ss.start();

    return app.exec();
}
