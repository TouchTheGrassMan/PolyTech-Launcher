#pragma once

#include <QImage>
#include <QPoint>
#include <QVector>
#include <QVector3D>
#include <QWidget>

/**
 * Software-rendered, rotatable 3D preview of a Minecraft skin.
 *
 * Builds the classic player box model (head, body, arms, legs) plus the second
 * ("overlay") layer, textures each face from the skin's UV regions, and draws
 * it with an orthographic projection using QPainter (painter's algorithm for
 * depth, no OpenGL). Drag with the left mouse button to rotate.
 *
 * Targets 64x64 skins; 64x32 legacy skins degrade gracefully (left limbs mirror
 * the right ones, only the hat overlay is shown).
 */
class SkinPreview3D : public QWidget {
    Q_OBJECT
   public:
    explicit SkinPreview3D(QWidget* parent = nullptr);

    void setSkin(const QImage& skin, bool slim);
    void clearSkin();

    QSize sizeHint() const override;

   protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

   private:
    struct Box {
        QVector3D center;
        QVector3D size;
        QPoint baseUV;
        QPoint overlayUV;
        bool overlay;
    };

    void rebuildBoxes();

    QImage m_skin;
    bool m_slim = false;
    QVector<Box> m_boxes;

    double m_yaw = -25.0;   // degrees, left-right
    double m_pitch = 12.0;  // degrees, up-down
    QPoint m_lastPos;
};
