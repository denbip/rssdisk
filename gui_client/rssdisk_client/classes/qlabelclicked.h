#ifndef QLABLECLICKED_H
#define QLABLECLICKED_H

#include <QObject>
#include <QLabel>
#include <QtDebug>

class QLabelClicked : public QLabel
{
public:
    QLabelClicked();

    Q_OBJECT
public:
    QLabelClicked( QWidget * parent = 0 );
    ~QLabelClicked(){}

signals:
    void clicked();

protected:
    void mousePressEvent ( QMouseEvent * event ) ;

};

#endif // QLABLECLICKED_H
