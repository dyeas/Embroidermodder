#include "object-polyline.h"
#include "object-data.h"

#include <QPainter>
#include <QStyleOption>
#include <QGraphicsScene>

PolylineObject::PolylineObject(qreal x, qreal y, const QPainterPath& p, QRgb rgb, QGraphicsItem* parent) : BaseObject(parent)
{
    qDebug("PolylineObject Constructor()");
    init(x, y, p, rgb, Qt::SolidLine); //TODO: getCurrentLineType
}

PolylineObject::PolylineObject(PolylineObject* obj, QGraphicsItem* parent) : BaseObject(parent)
{
    qDebug("PolylineObject Constructor()");
    if(obj)
    {
        init(obj->objectX(), obj->objectY(), obj->objectPath(), obj->objectColorRGB(), Qt::SolidLine); //TODO: getCurrentLineType
    }
}

PolylineObject::~PolylineObject()
{
    qDebug("PolylineObject Destructor()");
}

void PolylineObject::init(qreal x, qreal y, const QPainterPath& p, QRgb rgb, Qt::PenStyle lineType)
{
    setData(OBJ_TYPE, type());
    setData(OBJ_NAME, OBJ_NAME_POLYLINE);

    //WARNING: DO NOT enable QGraphicsItem::ItemIsMovable. If it is enabled,
    //WARNING: and the item is double clicked, the scene will erratically move the item while zooming.
    //WARNING: All movement has to be handled explicitly by us, not by the scene.
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    updatePath(p);
    setObjectPos(x,y);
    setObjectColor(rgb);
    setObjectLineType(lineType);
    setObjectLineWeight(0.35); //TODO: pass in proper lineweight
    setPen(objectPen());
}

void PolylineObject::updatePath(const QPainterPath& p)
{
    normalPath = p;
    QPainterPath reversePath = normalPath.toReversed();
    reversePath.connectPath(normalPath);
    setObjectPath(reversePath);
}

void PolylineObject::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/)
{
    QGraphicsScene* objScene = scene();
    if(!objScene) return;

    QPen paintPen = pen();
    if(option->state & QStyle::State_Selected)  { paintPen.setStyle(Qt::DashLine); }
    if(objScene->property(ENABLE_LWT).toBool()) { paintPen = lineWeightPen(); }
    painter->setPen(paintPen);

    updateRubber(painter);

    painter->drawPath(objectPath());

    //TODO: This is the initial concept for what realistic rendering be like. It's somewhat decent but needs improvement.
    if(objScene->property(ENABLE_LWT).toBool() && objScene->property(ENABLE_REAL).toBool())
    {
        int count = normalPath.elementCount();
        for(int i = 0; i < count-1; ++i)
        {
            QPainterPath::Element elem = normalPath.elementAt(i);
            QPainterPath::Element next = normalPath.elementAt(i+1);

            if(next.isMoveTo()) continue;

            QPainterPath elemPath;
            elemPath.moveTo(elem.x, elem.y);
            elemPath.lineTo(next.x, next.y);

            painter->setPen(objectColor().darker(150)); //TODO: Improve this for black and dark colors
            QPainterPathStroker stroker;
            stroker.setWidth(0.35);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            QPainterPath realPath = stroker.createStroke(elemPath);
            painter->drawPath(realPath);

            QLinearGradient grad(elemPath.pointAtPercent(0.5), elemPath.pointAtPercent(0.0));
            grad.setColorAt(0, objectColor());
            grad.setColorAt(1, objectColor().darker(150)); //TODO: Improve this for black and dark colors
            grad.setSpread(QGradient::ReflectSpread);

            painter->fillPath(realPath, QBrush(grad));
        }
    }
}

void PolylineObject::updateRubber(QPainter* painter)
{
    //TODO: Polyline Rubber Modes
}

void PolylineObject::vulcanize()
{
    qDebug("PolylineObject vulcanize()");
    updateRubber();

    setObjectRubberMode(OBJ_RUBBER_OFF);
}

// Returns the closest snap point to the mouse point
QPointF PolylineObject::mouseSnapPoint(const QPointF& mousePoint)
{
    QPointF closestPoint = scenePos();
    qreal closestDist = QLineF(mousePoint, closestPoint).length();
    QPainterPath::Element element;
    for(int i = 0; i < normalPath.elementCount(); ++i)
    {
        element = normalPath.elementAt(i);
        QPointF elemPoint = mapToScene(element.x, element.y);
        qreal elemDist = QLineF(mousePoint, elemPoint).length();
        if(elemDist < closestDist)
        {
            closestPoint = elemPoint;
            closestDist = elemDist;
        }
    }
    return closestPoint;
}

QList<QPointF> PolylineObject::allGripPoints()
{
    QList<QPointF> gripPoints;
    QPainterPath::Element element;
    for(int i = 0; i < normalPath.elementCount(); ++i)
    {
        element = normalPath.elementAt(i);
        gripPoints << mapToScene(element.x, element.y);
    }
    return gripPoints;
}

/* kate: bom off; indent-mode cstyle; indent-width 4; replace-trailing-space-save on; */
