#ifndef EMB_STITCH_H
#define EMB_STITCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Machine codes for stitch flags */
#define NORMAL              0 /* stitch to (xx, yy) */
#define JUMP                1 /* move to(xx, yy) */
#define TRIM                2 /* trim + move to(xx, yy) */
#define STOP                4 /* pause machine for thread change */
#define SEQUIN              8 /* sequin */
#define END                 16 /* end of program */

typedef struct EmbStitch_
{
    int flags; /* uses codes defined above */
    double xx; /* absolute position (not relative) */
    double yy;
    int color; /* color number for this stitch */
} EmbStitch;

typedef struct EmbStitchList_
{
    struct EmbStitch_ stitch;
    struct EmbStitchList_* next;
} EmbStitchList;

void embStitchList_add(EmbStitchList* pointer, EmbStitch data);
int embStitchList_count(EmbStitchList* pointer);
int embStitchList_empty(EmbStitchList* pointer);
/* TODO: Shouldn't there be a embStitchList_free function? */
EmbStitch embStitchList_getAt(EmbStitchList* pointer, int num);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EMB_STITCH_H */

/* kate: bom off; indent-mode cstyle; indent-width 4; replace-trailing-space-save on; */
