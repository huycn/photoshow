#include "imagewidget.h"
#include <QPainter>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <iostream>

static const int kAnimationFPS = 30;
static const float kBackgroundDarken = 0.6f;

ImageWidget::ImageWidget(QWidget* parent)
: QOpenGLWidget(parent)
{
    m_animeTimer = new QTimer(this);
    connect(m_animeTimer, &QTimer::timeout, this, QOverload<>::of(&ImageWidget::update));
}

ImageWidget::~ImageWidget()
{
}

void ImageWidget::closeEvent(QCloseEvent* event)
{
    QOpenGLWidget::closeEvent(event);
    emit closed();
}

static const char *vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char *fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D image;
uniform float opacity;

void main() {
    vec4 textureColor = texture(image, TexCoord);
    FragColor = vec4(textureColor.rgb, textureColor.a * opacity);
}
)";

void ImageWidget::initializeGL()
{
    initializeOpenGLFunctions();

    m_shader = new QOpenGLShaderProgram(this);
    m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSrc);
    m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSrc);
    m_shader->link();

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();

    float vertices[] = {
        // pos      // texture coords
        0.0f, 0.0f, 0.0f, 0.0f, // Bottom Left
        1.0f, 0.0f, 1.0f, 0.0f, // Bottom Right
        1.0f, 1.0f, 1.0f, 1.0f, // Top Right
        0.0f, 1.0f, 0.0f, 1.0f  // Top Left
    };
    m_vbo.allocate(vertices, sizeof(vertices));

    m_shader->enableAttributeArray(0);
    m_shader->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));

    m_shader->enableAttributeArray(1);
    m_shader->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));

    m_vbo.release();
    m_vao.release();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    fmt.setTextureTarget(GL_TEXTURE_2D);

    int w = width();
    int h = height();
    m_bgFbo.reset(new QOpenGLFramebufferObject(w, h, fmt));

    m_bgFbo->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    m_bgFbo->release();

    emit ready(w, h);
}

void ImageWidget::loadImage(const QImage &img, int x, int y, int w, int h)
{
    if (m_shader == nullptr) {
        return;
    }

    m_imageRect.setRect(x, y, w, h);
    m_image.reset(new QOpenGLTexture(img.convertToFormat(QImage::Format_RGBA8888)));
    m_image->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    m_image->setMagnificationFilter(QOpenGLTexture::Linear);
    m_image->setWrapMode(QOpenGLTexture::ClampToEdge);

    startAnimation();
}

void ImageWidget::drawTexturedQuad(float x, float y, float w, float h, float opacity)
{
    if (m_image == nullptr || !m_shader->isLinked()) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QMatrix4x4 mvp;
    mvp.ortho(0.0f, width(), height(), 0.0f, -1.0f, 1.0f);
    mvp.translate(x, y);
    mvp.scale(w, h);

    m_shader->bind();
    m_image->bind();
    m_shader->setUniformValue("image", 0);
    m_shader->setUniformValue("opacity", opacity);
    m_shader->setUniformValue("projection", mvp);

    m_vao.bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    m_vao.release();

    m_image->release();
    m_shader->release();

    glDisable(GL_BLEND);
}

void ImageWidget::resizeGL(int w, int h)
{
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    fmt.setTextureTarget(GL_TEXTURE_2D);

    std::unique_ptr<QOpenGLFramebufferObject> newFbo(new QOpenGLFramebufferObject(w, h, fmt));

    int prevHeight = m_bgFbo->height();
    QRect srcRect(0, 0, std::min(w, m_bgFbo->width()), std::min(h, prevHeight));
    if (srcRect.width() < w || srcRect.height() < h) {
        newFbo->bind();
        glClear(GL_COLOR_BUFFER_BIT);
        newFbo->release();
    }
    QRect destRect = srcRect;
    if (h > prevHeight) {
        // we want content to stick to uper-left corner
        destRect.translate(0, h - prevHeight);
    } else if (h < prevHeight) {
        srcRect.translate(0, prevHeight - h);
    }

    QOpenGLFramebufferObject::blitFramebuffer(newFbo.get(), destRect, m_bgFbo.get(), srcRect, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    m_bgFbo.reset(newFbo.release());
    update();

    emit resized(w, h);
}

void ImageWidget::startAnimation()
{
    m_animeElapsed.restart();
    m_animeTimer->start(1000 / kAnimationFPS);
}

void ImageWidget::stopAnimation()
{
    QRect fboRect(0, 0, m_bgFbo->width(), m_bgFbo->height());
    QOpenGLFramebufferObject::blitFramebuffer(m_bgFbo.get(), fboRect, nullptr, fboRect, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    m_image.reset();
    m_animeTimer->stop();
}

void ImageWidget::paintGL()
{
    if (!QOpenGLFramebufferObject::hasOpenGLFramebufferBlit()) {
        std::cerr << "No FramebufferBlit available\n";
        return;
    }

    QRect fboRect(0, 0, m_bgFbo->width(), m_bgFbo->height());
    QOpenGLFramebufferObject::blitFramebuffer(nullptr, fboRect, m_bgFbo.get(), fboRect, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    auto t = std::min(1.0f, m_animeElapsed.elapsed() / 1000.0f);

    if (m_image != nullptr) {
        {
            QPainter(this).fillRect(fboRect, QColor(0, 0, 0, static_cast<int>(t * kBackgroundDarken * 255)));
        }
        drawTexturedQuad(m_imageRect.left(), m_imageRect.top(), m_imageRect.width(), m_imageRect.height(), t);
    }

    if (t >= 1.0f) {
        stopAnimation();
    }
}
