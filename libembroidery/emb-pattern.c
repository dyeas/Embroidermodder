#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "emb-pattern.h"
#include "emb-reader-writer.h"
#include "emb-settings.h"
#include "helpers-misc.h"

EmbPattern* embPattern_create(void)
{
    EmbPattern* p = 0;
    p = (EmbPattern*)malloc(sizeof(EmbPattern));
    if(!p) { /* TODO: error */ return 0; }

    p->settings = embSettings_init();
    p->currentColorIndex = 0;
    p->stitchList = 0;
    p->threadList = 0;

    p->hoop.height = 0.0;
    p->hoop.width = 0.0;
    p->arcObjList = 0;
    p->circleObjList = 0;
    p->ellipseObjList = 0;
    p->lineObjList = 0;
    p->pathObjList = 0;
    p->pointObjList = 0;
    p->polygonObjList = 0;
    p->polylineObjList = 0;
    p->rectObjList = 0;
    p->splineObjList = 0;

    p->lastStitch = 0;
    p->lastThread = 0;

    p->lastArcObj = 0;
    p->lastCircleObj = 0;
    p->lastLineObj = 0;
    p->lastEllipseObj = 0;
    p->lastPathObj = 0;
    p->lastPointObj = 0;
    p->lastPolygonObj = 0;
    p->lastPolylineObj = 0;
    p->lastRectObj = 0;
    p->lastSplineObj = 0;

    p->lastX = 0.0;
    p->lastY = 0.0;
    return p;
}

void embPattern_hideStitchesOverLength(EmbPattern* p, int length)
{
    /* TODO: pointer safety */
    double prevX = 0;
    double prevY = 0;
    EmbStitchList* pointer = 0;
    pointer = p->stitchList;
    while(pointer)
    {
        if((fabs(pointer->stitch.xx - prevX) > length) || (fabs(pointer->stitch.yy - prevY) > length))
        {
            pointer->stitch.flags |= TRIM;
            pointer->stitch.flags &= ~NORMAL;
        }
        prevX = pointer->stitch.xx;
        prevY = pointer->stitch.yy;
        pointer = pointer->next;
    }
}

int embPattern_addThread(EmbPattern* p, EmbThread thread)
{
    /* TODO: pointer safety */
    if(!(p->threadList))
    {
        EmbThreadList* t = (EmbThreadList*)malloc(sizeof(EmbThreadList));
        if(!t) return 0;
        t->thread = thread;
        t->next = NULL;
        p->threadList = t;
    }
    else
    {
        embThreadList_add(p->threadList, thread);
    }
    return 1;
}

void embPattern_fixColorCount(EmbPattern* p)
{
    /* TODO: pointer safety */

    /* fix color count to be max of color index. */
    int maxColorIndex = 0;
    EmbStitchList* list = p->stitchList;
    while(list)
    {
        maxColorIndex = max(maxColorIndex, list->stitch.color);
        list = list->next;
    }
    while((int)embThreadList_count(p->threadList) <= maxColorIndex)
    {
        embPattern_addThread(p, embThread_getRandom());
    }
    /*
    while(embThreadList_count(p->threadList) > (maxColorIndex + 1))
    {
        TODO: erase last color    p->threadList.pop_back();
    }
    */
}

void moveStitchListToPolyline(EmbPattern* p)
{
    /* TODO: pointer safety */
    EmbStitchList* stitches = p->stitchList;
    EmbPolylineObjectList* currentList = 0;
    while(stitches)
    {
        EmbPointList* currentPointList = 0;
        EmbPolylineObject* currentPolyline = (EmbPolylineObject*)malloc(sizeof(EmbPolylineObject));
        /* TODO: malloc fail error */
        currentPolyline->pointList = 0;
        currentPolyline->lineType = 1; /* TODO: Determine what the correct value should be */
        currentPolyline->color = embThreadList_getAt(p->threadList, stitches->stitch.color).color;

        while(stitches)
        {
            if(stitches->stitch.flags & (STOP | TRIM))
            {
                break;
            }
            if(!(stitches->stitch.flags & JUMP))
            {
                if(!currentPointList)
                {
                    currentPointList = embPointList_create(stitches->stitch.xx, stitches->stitch.yy);
                    currentPolyline->pointList = currentPointList;
                }
                else
                {
                    currentPointList = embPointList_add(currentPointList, embPoint_make(stitches->stitch.xx, stitches->stitch.yy));
                }
            }
            stitches = stitches->next;
        }
        currentPointList = 0;
        if(!currentList)
        {
            currentList = embPolylineObjectList_create(currentPolyline);
            p->polylineObjList = currentList;
        }
        else
        {
            currentList = embPolylineObjectList_add(currentList, currentPolyline);
        }
        if(stitches && stitches->next)
        {
            stitches = stitches->next;
        }
    }
}

void movePolylinesToStitchList(EmbPattern* p)
{
    if(p)
    {
        EmbPolylineObjectList* polyList = p->polylineObjList;
        EmbStitchList* currentList = 0;
        int firstObject = 1;
        /*int currentColor = polyList->polylineObj->color TODO: polyline color */
        while(polyList)
        {
            EmbPolylineObject* currentPoly = polyList->polylineObj;
            EmbPointList* currentPointList = currentPoly->pointList;
            EmbThread thread;
            thread.catalogNumber = 0;
            thread.color = currentPoly->color;
            thread.description = 0;
            embPattern_addThread(p, thread);
            if(!firstObject)
            {
                embPattern_addStitchRel(p, currentPointList->point.xx, currentPointList->point.yy, TRIM, 1);
                embPattern_addStitchRel(p, 0.0, 0.0, STOP, 1);
                firstObject = 0;
            }
            while(currentPointList)
            {
                embPattern_addStitchAbs(p, currentPointList->point.xx, currentPointList->point.yy, NORMAL, 1);
                currentPointList = currentPointList->next;
            }

            polyList = polyList->next;
        }
        embPattern_addStitchRel(p, 0.0, 0.0, END, 1);
    }
}

/* Three routines to add a stitch to the pattern depending on format... */
void embPattern_addStitchAbs(EmbPattern* p, double x, double y, int flags, int isAutoColorIndex)
{
    /* TODO: pointer safety */
    EmbStitch s;

    if(flags & END)
    {

        embPattern_fixColorCount(p);

        /* HideStitchesOverLength(127); */
    }

    if((flags & STOP) && embStitchList_empty(p->stitchList))
        return;
    if((flags & STOP) && isAutoColorIndex)
    {
        p->currentColorIndex++;
    }

    s.xx = x;
    s.yy = y;
    s.flags = flags;
    s.color = p->currentColorIndex;
    if(!(p->stitchList))
    {
        p->stitchList = (EmbStitchList*)malloc(sizeof(EmbStitchList));
        /* TODO: malloc fail error */
        p->stitchList->stitch = s;
        p->stitchList->next = 0;
        p->lastStitch = p->stitchList;
    }
    else
    {
        embStitchList_add(p->lastStitch, s);
        p->lastStitch = p->lastStitch->next;
    }
    p->lastX = s.xx;
    p->lastY = s.yy;
}

void embPattern_addStitchRel(EmbPattern* p, double dx, double dy, int flags, int isAutoColorIndex)
{
    /* TODO: pointer safety */
    double x,y;
    if(!embStitchList_empty(p->stitchList))
    {
        x = p->lastX + dx;
        y = p->lastY + dy;
    }
    else
    {
        /* the list is empty so assume starting location is 0,0 */
        x = dx;
        y = dy;
    }
    embPattern_addStitchAbs(p, x, y, flags, isAutoColorIndex);
}

void embPattern_changeColor(EmbPattern* p, int index)
{
    /* TODO: pointer safety */
    p->currentColorIndex = index;
}

int embPattern_read(EmbPattern* p, const char* fileName) /* TODO: This doesn't work. Write test case using this convenience function. */
{
    EmbReaderWriter* reader = 0;
    embPattern_free(p);
    p = embPattern_create();
    reader = embReaderWriter_getByFileName(fileName);
    if(reader->reader(p, fileName))
    {
        free(reader);
        return 1;
    }
    free(reader);
    return 0;
}

int embPattern_write(EmbPattern* p, const char *fileName) /* TODO: Write test case using this convenience function. */
{
    EmbReaderWriter* writer = 0;
    writer = embReaderWriter_getByFileName(fileName);
    if(writer->writer(p, fileName))
    {
        free(writer);
        return 1;
    }
    free(writer);
    return 0;
}

/* Very simple scaling of the x and y axis for every point.
* Doesn't insert or delete stitches to preserve density. */
void embPattern_scale(EmbPattern* p, double scale)
{
    /* TODO: pointer safety */
    EmbStitchList* pointer = 0;
    pointer = p->stitchList;
    while(pointer)
    {
        pointer->stitch.xx *= scale;
        pointer->stitch.yy *= scale;
        pointer = pointer->next;
    }
}

EmbRect embPattern_calcBoundingBox(EmbPattern* p)
{
    /* TODO: pointer safety */
    EmbStitchList* pointer = 0;
    EmbRect BoundingRect;
    EmbStitch pt;
    EmbCircleObjectList* cObjList = 0;
    EmbCircle circle;
    EmbArcObjectList* aObjList = 0;
    EmbArc arc;
    EmbEllipseObjectList* eObjList = 0;
    EmbEllipse ellipse;
    EmbLineObjectList* liObjList = 0;
    EmbLine line;
    EmbPointObjectList* pObjList = 0;
    EmbPoint point;
    EmbPolygonObjectList* pogObjList = 0;
    EmbPoint pogPoint;
    EmbPolylineObjectList* polObjList = 0;
    EmbPointList* polyLinePointList = 0;
    EmbPoint polPoint;
    EmbRectObjectList* rObjList = 0;
    EmbRect rect;
    EmbSplineObjectList* sObjList = 0;
    EmbBezier bezier;
    /* Calculate the bounding rectangle.  It's needed for smart repainting. */
    /* TODO: Come back and optimize this mess so that after going thru all objects
            and stitches, if the rectangle isn't reasonable, then return a default rect */
    if(embStitchList_empty(p->stitchList) &&
    embArcObjectList_empty(p->arcObjList) &&
    embCircleObjectList_empty(p->circleObjList) &&
    embEllipseObjectList_empty(p->ellipseObjList) &&
    embLineObjectList_empty(p->lineObjList) &&
    embPointObjectList_empty(p->pointObjList) &&
    embPolygonObjectList_empty(p->polygonObjList) &&
    embPolylineObjectList_empty(p->polylineObjList) &&
    embRectObjectList_empty(p->rectObjList) &&
    embSplineObjectList_empty(p->splineObjList))
    {
        BoundingRect.top = 0.0;
        BoundingRect.left = 0.0;
        BoundingRect.bottom = 1.0;
        BoundingRect.right = 1.0;
        return BoundingRect;
    }
    BoundingRect.left = 99999.0;
    BoundingRect.top =  99999.0;
    BoundingRect.right = -99999.0;
    BoundingRect.bottom = -99999.0;

    pointer = p->stitchList;
    while(pointer)
    {
        /* If the point lies outside of the accumulated bounding
        * rectangle, then inflate the bounding rect to include it. */
        pt = pointer->stitch;
        if(!(pt.flags & TRIM))
        {
            BoundingRect.left = (double)min(BoundingRect.left, pt.xx);
            BoundingRect.top = (double)min(BoundingRect.top, pt.yy);
            BoundingRect.right = (double)max(BoundingRect.right, pt.xx);
            BoundingRect.bottom = (double)max(BoundingRect.bottom, pt.yy);
        }
        pointer = pointer->next;
    }

    aObjList = p->arcObjList;
    while(aObjList)
    {
        arc = aObjList->arcObj.arc;
        /* TODO: embPattern_calcBoundingBox for arcs */

        aObjList = aObjList->next;
    }

    cObjList = p->circleObjList;
    while(cObjList)
    {
        circle = cObjList->circleObj.circle;
        BoundingRect.left = (double)min(BoundingRect.left, circle.centerX - circle.radius);
        BoundingRect.top = (double)min(BoundingRect.top, circle.centerY - circle.radius);
        BoundingRect.right = (double)max(BoundingRect.right, circle.centerX + circle.radius);
        BoundingRect.bottom = (double)max(BoundingRect.bottom, circle.centerY + circle.radius);

        cObjList = cObjList->next;
    }

    eObjList = p->ellipseObjList;
    while(eObjList)
    {
        ellipse = eObjList->ellipseObj.ellipse;
        /* TODO: embPattern_calcBoundingBox for ellipses */

        eObjList = eObjList->next;
    }

    liObjList = p->lineObjList;
    while(liObjList)
    {
        line = liObjList->lineObj.line;
        /* TODO: embPattern_calcBoundingBox for lines */

        liObjList = liObjList->next;
    }

    pObjList = p->pointObjList;
    while(pObjList)
    {
        point = pObjList->pointObj.point;
        /* TODO: embPattern_calcBoundingBox for points */

        pObjList = pObjList->next;
    }

    pogObjList = p->polygonObjList;
    while(pogObjList)
    {
        pogPoint = pogObjList->polygonObj.polygonPoint;
        /* TODO: embPattern_calcBoundingBox for polygons */

        pogObjList = pogObjList->next;
    }

    polObjList = p->polylineObjList;
    while(polObjList)
    {
        polyLinePointList = polObjList->polylineObj->pointList;
        while(polyLinePointList)
        {
            polPoint = polyLinePointList->point;
            /* TODO: embPattern_calcBoundingBox for polylines */

            polyLinePointList = polyLinePointList->next;
        }
        polObjList = polObjList->next;
    }

    rObjList = p->rectObjList;
    while(rObjList)
    {
        rect = rObjList->rectObj.rect;
        /* TODO: embPattern_calcBoundingBox for rectangles */

        rObjList = rObjList->next;
    }

    sObjList = p->splineObjList;
    while(sObjList)
    {
        bezier = sObjList->splineObj.bezier;
        /* TODO: embPattern_calcBoundingBox for splines */

        sObjList = sObjList->next;
    }

    return BoundingRect;
}

void embPattern_flipVertical(EmbPattern* p)
{
    /* TODO: pointer safety */
    EmbStitchList* pointer = 0;
    pointer= p->stitchList;
    while(pointer)
    {
        pointer->stitch.yy = -pointer->stitch.yy;
        pointer = pointer->next;
    }
}

void embPattern_combineJumpStitches(EmbPattern* p)
{
    /* TODO: pointer safety */
    EmbStitchList* pointer = 0;
    int jumpCount = 0;
    EmbStitchList* jumpListStart = 0;
	pointer = p->stitchList;
    while(pointer)
    {
        if(pointer->stitch.flags & JUMP)
        {
            if(jumpCount == 0)
            {
                jumpListStart = pointer;
            }
            jumpCount++;
        }
        else
        {
            if(jumpCount > 0)
            {
                EmbStitchList* removePointer = jumpListStart->next;
                jumpListStart->stitch.xx = pointer->stitch.xx;
                jumpListStart->stitch.yy = pointer->stitch.yy;
                jumpListStart->next = pointer;

                for(; jumpCount > 0; jumpCount--)
                {
                    EmbStitchList* tempPointer = removePointer->next;
                    free(removePointer);
                    removePointer = tempPointer;
                }
                jumpCount = 0;
            }
        }
        pointer = pointer->next;
    }
}

void embPattern_correctForMaxStitchLength(EmbPattern* p, double maxStitchLength, double maxJumpLength)
{
    /* TODO: pointer safety */
    int j = 0, splits;
    double maxXY, maxLen, addX, addY;

    if(embStitchList_count(p->stitchList) > 1)
    {
        EmbStitchList* pointer = 0;
        EmbStitchList* prev = 0;
        prev = p->stitchList;
        pointer = prev->next;

        while(pointer)
        {
            double xx = prev->stitch.xx;
            double yy = prev->stitch.yy;
            double dx = pointer->stitch.xx - xx;
            double dy = pointer->stitch.yy - yy;
            if((fabs(dx) > maxStitchLength) || (fabs(dy) > maxStitchLength))
            {
                maxXY = max(fabs(dx), fabs(dy));
                if(pointer->stitch.flags & (JUMP | TRIM)) maxLen = maxJumpLength;
                else maxLen = maxStitchLength;

                splits = (int)ceil((double)maxXY / maxLen);

                if(splits > 1)
                {
                    int flagsToUse = pointer->stitch.flags;
                    int colorToUse = pointer->stitch.color;
                    addX = (double)dx / splits;
                    addY = (double)dy / splits;

                    for(j = 1; j < splits; j++)
                    {
                        EmbStitchList *item;
                        EmbStitch s;
                        s.xx = xx + addX * j;
                        s.yy = yy + addY * j;
                        s.flags = flagsToUse;
                        s.color = colorToUse;
                        item = (EmbStitchList *)malloc(sizeof(EmbStitchList));
                        /* TODO: malloc fail error */
                        item->stitch = s;
                        item->next = pointer;
                        prev->next = item;
                        prev = item;
                    }
                }
            }
            prev = pointer;
            if(pointer)
            {
                pointer = pointer->next;
            }
        }
    }
    if(p->lastStitch && p->lastStitch->stitch.flags != END)
    {
        embPattern_addStitchAbs(p, p->lastStitch->stitch.xx, p->lastStitch->stitch.yy, END, 1);
    }
}

void embPattern_center(EmbPattern* p)
{
    /* TODO: pointer safety */

    /* TODO: review this. currently not used in anywhere. Also needs to handle various design objects */
    int moveLeft, moveTop;
    EmbRect boundingRect;
    EmbStitchList* pointer = 0;

    boundingRect = embPattern_calcBoundingBox(p);

    moveLeft = (int)(boundingRect.left - (embRect_width(boundingRect) / 2.0));
    moveTop = (int)(boundingRect.top - (embRect_height(boundingRect) / 2.0));

    pointer = p->stitchList;
    while(pointer)
    {
        EmbStitch s;
        s = pointer->stitch;
        s.xx -= moveLeft;
        s.yy -= moveTop;
    }
}

void embPattern_loadExternalColorFile(EmbPattern* p, const char* fileName)
{
    /* TODO: pointer safety */
    char hasRead = 0;
    EmbReaderWriter* colorfile = 0;
    const char* dotPos = strrchr(fileName, '.');

    char* extractName = (char*)malloc(dotPos - fileName + 5);
    if(!extractName) return;
    extractName = (char *)memcpy(extractName, fileName, dotPos - fileName);
    extractName[dotPos - fileName] = '\0';
    strcat(extractName,".edr");
    colorfile = embReaderWriter_getByFileName(extractName);
    if(colorfile)
    {
        hasRead = (char)colorfile->reader(p, extractName);
    }
    if(!hasRead)
    {
        free(colorfile);
        extractName = (char *)memcpy(extractName, fileName, dotPos - fileName);
        extractName[dotPos - fileName] = '\0';
        strcat(extractName,".rgb");
        colorfile = embReaderWriter_getByFileName(extractName);
        if(colorfile)
        {
            hasRead = (char)colorfile->reader(p, extractName);
        }
    }
    if(!hasRead)
    {
        free(colorfile);
        extractName = (char *)memcpy(extractName, fileName, dotPos - fileName);
        extractName[dotPos - fileName] = '\0';
        strcat(extractName,".col");
        colorfile = embReaderWriter_getByFileName(extractName);
        if(colorfile)
        {
            hasRead = (char)colorfile->reader(p, extractName);
        }
    }
    if(!hasRead)
    {
        free(colorfile);
        extractName = (char *)memcpy(extractName, fileName, dotPos - fileName);
        extractName[dotPos - fileName] = '\0';
        strcat(extractName,".inf");
        colorfile = embReaderWriter_getByFileName(extractName);
        if(colorfile)
        {
            hasRead = (char)colorfile->reader(p, extractName);
        }
    }
    free(colorfile);
    free(extractName);
}

void embPattern_free(EmbPattern* p)
{
    EmbThreadList* thisThreadList = 0;
    EmbThreadList* nextThreadList = 0;
    EmbStitchList* thisStitchList = 0;
    EmbStitchList* nextStitchList = 0;

    EmbArcObjectList* thisArcObjList = 0;
    EmbArcObjectList* nextArcObjList = 0;
    EmbCircleObjectList* thisCircleObjList = 0;
    EmbCircleObjectList* nextCircleObjList = 0;
    EmbEllipseObjectList* thisEllipseObjList = 0;
    EmbEllipseObjectList* nextEllipseObjList = 0;
    EmbLineObjectList* thisLineObjList = 0;
    EmbLineObjectList* nextLineObjList = 0;
    EmbPathObjectList* thisPathObjList = 0;
    EmbPathObjectList* nextPathObjList = 0;
    EmbPathObject* thisPathObj = 0;
    EmbPathObject* nextPathObj = 0;
    EmbPointObjectList* thisPointObjList = 0;
    EmbPointObjectList* nextPointObjList = 0;
    EmbPolygonObjectList* thisPolygonObjList = 0;
    EmbPolygonObjectList* nextPolygonObjList = 0;
    EmbPolylineObjectList* thisPolylineObjList = 0;
    EmbPolylineObjectList* nextPolylineObjList = 0;
    EmbRectObjectList* thisRectObjList = 0;
    EmbRectObjectList* nextRectObjList = 0;
    EmbSplineObjectList* thisSplineObjList = 0;
    EmbSplineObjectList* nextSplineObjList = 0;

    if(!p) return;
    thisStitchList = p->stitchList;
    while(thisStitchList)
    {
        nextStitchList = thisStitchList->next;
        free(thisStitchList);
        thisStitchList = nextStitchList;
    }
    thisThreadList = p->threadList;
    while(thisThreadList)
    {
        nextThreadList = thisThreadList->next;
        free(thisThreadList);
        thisThreadList = nextThreadList;
    }

    thisArcObjList = p->arcObjList;
    while(thisArcObjList)
    {
        nextArcObjList = thisArcObjList->next;
        free(thisArcObjList);
        thisArcObjList = nextArcObjList;
    }
    thisCircleObjList = p->circleObjList;
    while(thisCircleObjList)
    {
        nextCircleObjList = thisCircleObjList->next;
        free(thisCircleObjList);
        thisCircleObjList = nextCircleObjList;
    }
    thisEllipseObjList = p->ellipseObjList;
    while(thisEllipseObjList)
    {
        nextEllipseObjList = thisEllipseObjList->next;
        free(thisEllipseObjList);
        thisEllipseObjList = nextEllipseObjList;
    }
    thisLineObjList = p->lineObjList;
    while(thisLineObjList)
    {
        nextLineObjList = thisLineObjList->next;
        free(thisLineObjList);
        thisLineObjList = nextLineObjList;
    }

    /*
    thisPathObjList = p->pathObjList;
    while(thisPathObjList)
    {


        thisPathObj = &thisPathObjList->pathObj;
        while(thisPathObj)
        {
            nextPathObj = thisPathObj->next;
            free(thisPathObj);
            thisPathObj = nextPathObj;
        }





        nextPathObjList = thisPathObjList->next;
        free(thisPathObjList);
        thisPathObjList = nextPathObjList;
    }
    */

    thisPointObjList = p->pointObjList;
    while(thisPointObjList)
    {
        nextPointObjList = thisPointObjList->next;
        free(thisPointObjList);
        thisPointObjList = nextPointObjList;
    }
    thisPolygonObjList = p->polygonObjList;
    while(thisPolygonObjList)
    {
        nextPolygonObjList = thisPolygonObjList->next;
        free(thisPolygonObjList);
        thisPolygonObjList = nextPolygonObjList;
    }
    thisPolylineObjList = p->polylineObjList;
    while(thisPolylineObjList)
    {
        nextPolylineObjList = thisPolylineObjList->next;
        embPointList_free(thisPolylineObjList->polylineObj->pointList);
        free(thisPolylineObjList);
        thisPolylineObjList = nextPolylineObjList;
    }
    thisRectObjList = p->rectObjList;
    while(thisRectObjList)
    {
        nextRectObjList = thisRectObjList->next;
        free(thisRectObjList);
        thisRectObjList = nextRectObjList;
    }
    thisSplineObjList = p->splineObjList;
    while(thisSplineObjList)
    {
        nextSplineObjList = thisSplineObjList->next;
        free(thisSplineObjList);
        thisSplineObjList = nextSplineObjList;
    }
    free(p);
    p = 0;
}

void embPattern_addCircleObjectAbs(EmbPattern* p, double cx, double cy, double r)
{
    /* TODO: pointer safety */
    EmbCircleObject circleObj = embCircleObject_make(cx, cy, r);

    if(!(p->circleObjList))
    {
        p->circleObjList = (EmbCircleObjectList*)malloc(sizeof(EmbCircleObjectList));
        /* TODO: malloc fail error */
        p->circleObjList->circleObj = circleObj;
        p->circleObjList->next = 0;
        p->lastCircleObj = p->circleObjList;
    }
    else
    {
        embCircleObjectList_add(p->lastCircleObj, circleObj);
        p->lastCircleObj = p->lastCircleObj->next;
    }
}

void embPattern_addEllipseObjectAbs(EmbPattern* p, double cx, double cy, double rx, double ry)
{
    /* TODO: pointer safety */
    EmbEllipseObject ellipseObj = embEllipseObject_make(cx, cy, rx, ry);

    if(!(p->ellipseObjList))
    {
        p->ellipseObjList = (EmbEllipseObjectList*)malloc(sizeof(EmbEllipseObjectList));
        /* TODO: malloc fail error */
        p->ellipseObjList->ellipseObj = ellipseObj;
        p->ellipseObjList->next = 0;
        p->lastEllipseObj = p->ellipseObjList;
    }
    else
    {
        embEllipseObjectList_add(p->lastEllipseObj, ellipseObj);
        p->lastEllipseObj = p->lastEllipseObj->next;
    }
}

void embPattern_addLineObjectAbs(EmbPattern* p, double x1, double y1, double x2, double y2)
{
    /* TODO: pointer safety */
    EmbLineObject lineObj = embLineObject_make(x1, y1, x2, y2);

    if(!(p->lineObjList))
    {
        p->lineObjList = (EmbLineObjectList*)malloc(sizeof(EmbLineObjectList));
        /* TODO: malloc fail error */
        p->lineObjList->lineObj = lineObj;
        p->lineObjList->next = 0;
        p->lastLineObj = p->lineObjList;
    }
    else
    {
        embLineObjectList_add(p->lastLineObj, lineObj);
        p->lastLineObj = p->lastLineObj->next;
    }
}

void embPattern_addPathObjectAbs(EmbPattern* pattern, EmbPathObject* pathObj)
{
    /* TODO: pointer safety */
}

void embPattern_addPointObjectAbs(EmbPattern* pattern, double x, double y)
{
    /* TODO: pointer safety */
    EmbPointObject pointObj = embPointObject_make(x, y);

    if(!(pattern->pointObjList))
    {
        pattern->pointObjList = (EmbPointObjectList*)malloc(sizeof(EmbPointObjectList));
        /* TODO: malloc fail error */
        pattern->pointObjList->pointObj = pointObj;
        pattern->pointObjList->next = 0;
        pattern->lastPointObj = pattern->pointObjList;
    }
    else
    {
        embPointObjectList_add(pattern->lastPointObj, pointObj);
        pattern->lastPointObj = pattern->lastPointObj->next;
    }
}

void embPattern_addPolylineObjectAbs(EmbPattern* pattern, EmbPolylineObject* obj)
{
    if(!(pattern->polylineObjList))
    {
        pattern->polylineObjList = (EmbPolylineObjectList*)malloc(sizeof(EmbPolylineObjectList));
        /* TODO: malloc fail error */
        pattern->polylineObjList->polylineObj = obj;
        pattern->polylineObjList->next = 0;
        pattern->lastPolylineObj = pattern->polylineObjList;
    }
    else
    {
        embPolylineObjectList_add(pattern->lastPolylineObj, obj);
        pattern->lastPolylineObj = pattern->lastPolylineObj->next;
    }
}

void embPattern_addRectObjectAbs(EmbPattern* p, double x, double y, double w, double h)
{
    /* TODO: pointer safety */
    EmbRectObject rectObj = embRectObject_make(x, y, w, h);

    if(!(p->rectObjList))
    {
        p->rectObjList = (EmbRectObjectList*)malloc(sizeof(EmbRectObjectList));
        /* TODO: malloc fail error */
        p->rectObjList->rectObj = rectObj;
        p->rectObjList->next = 0;
        p->lastRectObj = p->rectObjList;
    }
    else
    {
        embRectObjectList_add(p->lastRectObj, rectObj);
        p->lastRectObj = p->lastRectObj->next;
    }
}

/* kate: bom off; indent-mode cstyle; indent-width 4; replace-trailing-space-save on; */
