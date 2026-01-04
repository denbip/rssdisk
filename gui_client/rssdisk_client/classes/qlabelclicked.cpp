#include "qlabelclicked.h"

QLabelClicked::QLabelClicked(QWidget * parent ) : QLabel(parent)
{

}

void QLabelClicked::mousePressEvent ( QMouseEvent * event )
{
    emit clicked();
}
