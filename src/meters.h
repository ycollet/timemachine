#ifndef METERS_H
#define METERS_H

#define lin2db(lin) (20.0f * log10f(fmaxf((lin), 1e-6f)))
#define db2lin(db)  (powf(10.0f, (db) / 20.0f))

void bind_meters(void);

void update_meters(float amp[]);

#endif
