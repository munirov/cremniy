#include "ptyprocess.h"

#ifdef Q_OS_WIN
#include "ptyprocess_windows.h"
#else
#include "ptyprocess_posix.h"
#endif

namespace Cremniy::Terminal {

PtyProcess *createPtyProcess(QObject *parent)
{
#ifdef Q_OS_WIN
    return new PtyProcessWindows(parent);
#else
    return new PtyProcessPosix(parent);
#endif
}

} // namespace Cremniy::Terminal
