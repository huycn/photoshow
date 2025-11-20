#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QElapsedTimer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

class QOpenGLTexture;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

class ImageWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit ImageWidget(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~ImageWidget();
    void loadImage(const QImage &img, int x, int y, int w, int h);

signals:
    void ready(int w, int h);
    void closed();
    void resized(int w, int h);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void drawTexturedQuad(float x, float y, float w, float h, float opacity);
    void startAnimation();
    void stopAnimation();

    std::unique_ptr<QOpenGLTexture> m_image;
    QRect m_imageRect;
    std::unique_ptr<QOpenGLFramebufferObject> m_bgFbo;
    QOpenGLShaderProgram* m_shader;
    QTimer* m_animeTimer;
    QElapsedTimer m_animeElapsed;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
};
