#include "SkinPreview3D.h"

#include <algorithm>

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPolygonF>
#include <QRect>
#include <QTransform>
#include <QVector3D>

SkinPreview3D::SkinPreview3D(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(180, 220);
}

QSize SkinPreview3D::sizeHint() const
{
    return QSize(200, 240);
}

void SkinPreview3D::setSkin(const QImage& skin, bool slim)
{
    m_skin = skin.isNull() ? QImage() : skin.convertToFormat(QImage::Format_ARGB32);
    m_slim = slim;
    rebuildBoxes();
    update();
}

void SkinPreview3D::clearSkin()
{
    m_skin = QImage();
    m_boxes.clear();
    update();
}

void SkinPreview3D::rebuildBoxes()
{
    m_boxes.clear();
    if (m_skin.isNull())
        return;

    const bool legacy = m_skin.height() < 64;   // 64x32 old skins
    const double armW = m_slim ? 3.0 : 4.0;
    const double armCX = m_slim ? 5.5 : 6.0;     // arm centre offset from body centre

    // center, size, baseUV, overlayUV, hasOverlay
    // Head
    m_boxes.push_back({ QVector3D(0, 28, 0), QVector3D(8, 8, 8), QPoint(0, 0), QPoint(32, 0), true });
    // Body
    m_boxes.push_back({ QVector3D(0, 18, 0), QVector3D(8, 12, 4), QPoint(16, 16), QPoint(16, 32), !legacy });
    // Right arm (player's right -> -X, i.e. viewer's left when facing the model)
    m_boxes.push_back({ QVector3D(-armCX, 18, 0), QVector3D(armW, 12, 4), QPoint(40, 16), QPoint(40, 32), !legacy });
    // Left arm (+X)
    m_boxes.push_back({ QVector3D(armCX, 18, 0), QVector3D(armW, 12, 4),
                        legacy ? QPoint(40, 16) : QPoint(32, 48), QPoint(48, 48), !legacy });
    // Right leg (-X)
    m_boxes.push_back({ QVector3D(-2, 6, 0), QVector3D(4, 12, 4), QPoint(0, 16), QPoint(0, 32), !legacy });
    // Left leg (+X)
    m_boxes.push_back({ QVector3D(2, 6, 0), QVector3D(4, 12, 4),
                        legacy ? QPoint(0, 16) : QPoint(16, 48), QPoint(0, 48), !legacy });
}

void SkinPreview3D::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.fillRect(rect(), QColor(32, 34, 40));

    if (m_skin.isNull() || m_boxes.isEmpty()) {
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(rect(), Qt::AlignCenter, tr("Нет скина"));
        return;
    }

    const QVector3D pivot(0, 16, 0);
    QMatrix4x4 rot;
    rot.rotate(static_cast<float>(m_pitch), 1, 0, 0);
    rot.rotate(static_cast<float>(m_yaw), 0, 1, 0);

    const double scale = qMin(width(), height()) / 42.0;
    const double ox = width() / 2.0;
    const double oy = height() / 2.0;

    struct DrawFace {
        double z;
        QImage img;
        QPolygonF dst;
    };
    QVector<DrawFace> faces;

    auto V = [](double x, double y, double z) { return QVector3D(float(x), float(y), float(z)); };

    for (const Box& b : m_boxes) {
        const int layers = b.overlay ? 2 : 1;
        for (int layer = 0; layer < layers; ++layer) {
            const double inflate = (layer == 1) ? 0.6 : 0.0;
            const QPoint uvo = (layer == 1) ? b.overlayUV : b.baseUV;

            const double w = b.size.x(), h = b.size.y(), d = b.size.z();
            const int iw = int(w), ih = int(h), id = int(d);
            const int u = uvo.x(), v = uvo.y();

            const double a = b.center.x() - w / 2 - inflate;
            const double bx = b.center.x() + w / 2 + inflate;
            const double c = b.center.y() - h / 2 - inflate;
            const double dd = b.center.y() + h / 2 + inflate;
            const double e = b.center.z() - d / 2 - inflate;
            const double f = b.center.z() + d / 2 + inflate;

            struct FDef {
                QVector3D c0, c1, c2, c3;  // corners in texture order: TL, TR, BR, BL
                QRect uv;
            };
            const FDef fs[6] = {
                // Front (+Z)
                { V(a, dd, f), V(bx, dd, f), V(bx, c, f), V(a, c, f), QRect(u + id, v + id, iw, ih) },
                // Back (-Z)
                { V(bx, dd, e), V(a, dd, e), V(a, c, e), V(bx, c, e), QRect(u + id + iw + id, v + id, iw, ih) },
                // Right (-X)
                { V(a, dd, e), V(a, dd, f), V(a, c, f), V(a, c, e), QRect(u, v + id, id, ih) },
                // Left (+X)
                { V(bx, dd, f), V(bx, dd, e), V(bx, c, e), V(bx, c, f), QRect(u + id + iw, v + id, id, ih) },
                // Top (+Y)
                { V(a, dd, e), V(bx, dd, e), V(bx, dd, f), V(a, dd, f), QRect(u + id, v, iw, id) },
                // Bottom (-Y)
                { V(a, c, f), V(bx, c, f), V(bx, c, e), V(a, c, e), QRect(u + id + iw, v, iw, id) },
            };

            for (const FDef& fd : fs) {
                if (fd.uv.width() <= 0 || fd.uv.height() <= 0)
                    continue;
                const QVector3D corners[4] = { fd.c0, fd.c1, fd.c2, fd.c3 };
                QPolygonF dst;
                double zsum = 0;
                for (const QVector3D& corner : corners) {
                    const QVector3D r = rot.map(corner - pivot);
                    dst << QPointF(ox + r.x() * scale, oy - r.y() * scale);
                    zsum += r.z();
                }
                QImage sub = m_skin.copy(fd.uv);
                if (sub.isNull())
                    continue;
                faces.push_back({ zsum / 4.0, sub, dst });
            }
        }
    }

    // Painter's algorithm: far (smaller z) first, near (larger z) last.
    std::sort(faces.begin(), faces.end(), [](const DrawFace& A, const DrawFace& B) { return A.z < B.z; });

    for (const DrawFace& df : faces) {
        QPolygonF src;
        src << QPointF(0, 0) << QPointF(df.img.width(), 0) << QPointF(df.img.width(), df.img.height())
            << QPointF(0, df.img.height());
        QTransform t;
        if (!QTransform::quadToQuad(src, df.dst, t))
            continue;
        p.setWorldTransform(t);
        p.drawImage(0, 0, df.img);
    }
    p.resetTransform();
}

void SkinPreview3D::mousePressEvent(QMouseEvent* event)
{
    m_lastPos = event->position().toPoint();
}

void SkinPreview3D::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    const QPoint pos = event->position().toPoint();
    const QPoint delta = pos - m_lastPos;
    m_lastPos = pos;

    m_yaw += delta.x() * 0.6;
    m_pitch += delta.y() * 0.6;
    if (m_pitch > 89.0)
        m_pitch = 89.0;
    if (m_pitch < -89.0)
        m_pitch = -89.0;
    update();
}
