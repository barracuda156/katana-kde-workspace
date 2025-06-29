/*

    Copyright (C) 2001 The Kompany
                  2001-2003 Ilya Konstantinov <kde-devel@future.shiny.co.il>
                  2001-2008 Marcus Meissner <marcus@jet.franken.de>
                  2012 Marcus Meissner <marcus@jet.franken.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

// remove comment to enable debugging
// #undef QT_NO_DEBUG

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include <qfile.h>
#include <qtextstream.h>
#include <qcoreapplication.h>

#include <kdebug.h>
#include <kcomponentdata.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocale.h>
#include <kprotocolinfo.h>
#include <kio/global.h>
#include <kconfiggroup.h>

#include "kio_camera.h"

#define tocstr(x) ((x).toLocal8Bit())

#define MAXIDLETIME   30      /* seconds */

using namespace KIO;

extern "C"
{
    static void frontendCameraStatus(GPContext *context, const char *status, void *data);
    static unsigned int frontendProgressStart(GPContext *context, float totalsize, const char *status, void *data);
    static void frontendProgressUpdate(GPContext *context, unsigned int id, float current, void *data);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    KComponentData componentData("kio_kamera");

    if (argc != 2) {
        kDebug(7123) << "Usage: kio_kamera app-socket";
        exit(-1);
    }

    KameraProtocol slave(argv[1]);
    slave.dispatchLoop();

    return 0;
}

static QString path_quote(QString path)   { return path.replace("/","%2F").replace(" ","%20"); }
static QString path_unquote(QString path) { return path.replace("%2F","/").replace("%20"," "); }

KameraProtocol::KameraProtocol(const QByteArray &app)
    : SlaveBase("camera", app),
    m_camera(NULL)
{
    // attempt to initialize libgphoto2 and chosen camera (requires locking)
    // (will init m_camera, since the m_camera's configuration is empty)
    m_camera = 0;
    m_file = NULL;
    m_config = new KConfig(KProtocolInfo::config("camera"), KConfig::SimpleConfig);
    m_context = gp_context_new();
    actiondone = true;
    cameraopen = false;
    m_lockfile = KStandardDirs::locateLocal("tmp", "kamera");
    idletime = 0;
}

// This handler is getting called every second. We use it to do the
// delayed close of the camera.
// Logic is:
//  - No more requests in the queue (signaled by actiondone) AND
//  - We are MAXIDLETIME seconds idle OR
//  - Another slave wants to have access to the camera.
//
// The existence of a lockfile is used to signify "please give up camera".
//
void KameraProtocol::special(const QByteArray&) {
    kDebug(7123) << "KameraProtocol::special() at " << getpid();

    if (!actiondone && cameraopen) {
        struct stat stbuf;
        if (::stat(m_lockfile.toUtf8(), &stbuf) != -1 || idletime++ >= MAXIDLETIME) {
            kDebug(7123) << "KameraProtocol::special() closing camera.";
            closeCamera();
            setTimeoutSpecialCommand(-1);
        } else {
            // continue to wait
            setTimeoutSpecialCommand(1);
        }
    } else {
        // We let it run until the slave gets no actions anymore.
        setTimeoutSpecialCommand(1);
    }
    actiondone = false;
}

KameraProtocol::~KameraProtocol()
{
    kDebug(7123) << "KameraProtocol::~KameraProtocol()";
    delete m_config;
    if (m_camera) {
        closeCamera();
        gp_camera_free(m_camera);
        m_camera = NULL;
    }
}

// initializes the camera for usage - should be done before operations over the wire
bool KameraProtocol::openCamera(QString &str) {
    idletime = 0;
    actiondone = true;
    if (!m_camera) {
        reparseConfiguration();
    } else {
        if (!cameraopen) {
            int gpr, tries = 15;
            kDebug(7123) << "KameraProtocol::openCamera at " << getpid();
            while (tries--) {
                gpr = gp_camera_init(m_camera, m_context);
                if (gpr == GP_ERROR_IO_USB_CLAIM || gpr == GP_ERROR_IO_LOCK) {
                    // just create / touch if not there
                    int fd = ::open(m_lockfile.toUtf8(), O_CREAT|O_WRONLY, 0600);
                    if (fd != -1) {
                        ::close(fd);
                    }
                    ::sleep(1);
                    kDebug(7123) << "openCamera at " << ::getpid() << "- busy, ret " << gpr << ", trying again.";
                    continue;
                }
                if (gpr == GP_OK) {
                    break;
                }
                str = gp_result_as_string(gpr);
                return false;
            }
            ::unlink(m_lockfile.toUtf8());
            setTimeoutSpecialCommand(1);
            kDebug(7123) << "openCamera succeeded at " << getpid();
            cameraopen = true;
        }
    }
    return true;
}

// should be done after operations over the wire
void KameraProtocol::closeCamera(void)
{
    int gpr;

    if (!m_camera) {
        return;
    }

    kDebug(7123) << "KameraProtocol::closeCamera at " << getpid();
    gpr = gp_camera_exit(m_camera, m_context);
    if (gpr != GP_OK) {
        kDebug(7123) << "closeCamera failed with " << gp_result_as_string(gpr);
    }
    // HACK: gp_camera_exit() in gp 2.0 does not close the port if there
    //       is no camera_exit function.
    gp_port_close(m_camera->port);
    cameraopen = false;
    current_camera = "";
    current_port = "";
    return;
}

static QString fix_foldername(QString ofolder) {
    QString folder = ofolder;
    if (folder.length() > 1) {
        while (folder.length() > 1 && folder.right(1) == "/") {
            folder = folder.left(folder.length()-1);
        }
    }
    if (folder.length() == 0) {
        folder = "/";
    }
    return folder;
}

// The KIO slave "get" function (starts a download from the camera)
// The actual returning of the data is done in the frontend callback functions.
void KameraProtocol::get(const KUrl &url)
{
    kDebug(7123) << "KameraProtocol::get(" << url.path() << ")";
    QString directory, file;
    CameraFileType fileType;
    int gpr;

    split_url2camerapath(url.path(), directory, file);
    if (!openCamera()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }


#define GPHOTO_TEXT_FILE(xx)                                                                \
    if (directory == "/" && file == #xx ".txt") {                                           \
        CameraText xx;                                                                      \
        gpr = gp_camera_get_##xx(m_camera,  &xx, m_context);                                \
        if (gpr != GP_OK) {                                                                 \
            error(KIO::ERR_DOES_NOT_EXIST, url.path());                                     \
            return;                                                                         \
        }                                                                                   \
        QByteArray chunkDataBuffer = QByteArray::fromRawData(xx.text, strlen(xx.text));     \
        data(chunkDataBuffer);                                                              \
        processedSize(strlen(xx.text));                                                     \
        chunkDataBuffer.clear();                                                            \
        finished();                                                                         \
        return;                                                                             \
    }

    GPHOTO_TEXT_FILE(about);
    GPHOTO_TEXT_FILE(manual);
    GPHOTO_TEXT_FILE(summary);
#undef GPHOTO_TEXT_FILE

    // emit info message
    infoMessage(i18n("Retrieving data from camera <b>%1</b>", current_camera));

    // Note: There's no need to re-read directory for each get() anymore
    gp_file_new(&m_file);

    // emit the total size (we must do it before sending data to allow preview)
    CameraFileInfo info;

    gpr = gp_camera_file_get_info(m_camera, tocstr(fix_foldername(directory)), tocstr(file), &info, m_context);
    if (gpr != GP_OK) {
        gp_file_unref(m_file);
        if (gpr == GP_ERROR_FILE_NOT_FOUND || gpr == GP_ERROR_DIRECTORY_NOT_FOUND) {
            error(KIO::ERR_DOES_NOT_EXIST, url.path());
        } else {
            error(KIO::ERR_UNKNOWN, QString::fromLocal8Bit(gp_result_as_string(gpr)));
        }
        return;
    }

    // at last, a proper API to determine whether a thumbnail was requested.
    if(cameraSupportsPreview() && metaData("thumbnail") == "1") {
        kDebug(7123) << "get() retrieving the thumbnail";
        fileType = GP_FILE_TYPE_PREVIEW;
        if (info.preview.fields & GP_FILE_INFO_SIZE) {
            totalSize(info.preview.size);
        }
    } else {
        kDebug(7123) << "get() retrieving the full-scale photo";
        fileType = GP_FILE_TYPE_NORMAL;
        if (info.file.fields & GP_FILE_INFO_SIZE) {
            totalSize(info.file.size);
        }
    }

    // fetch the data
    m_fileSize = 0;
    gpr = gp_camera_file_get(m_camera, tocstr(fix_foldername(directory)), tocstr(file), fileType, m_file, m_context);
    if (gpr == GP_ERROR_NOT_SUPPORTED && fileType == GP_FILE_TYPE_PREVIEW) {
        // If we get here, the file info command information
        // will either not be used, or still valid.
        fileType = GP_FILE_TYPE_NORMAL;
        gpr = gp_camera_file_get(m_camera, tocstr(fix_foldername(directory)), tocstr(file), fileType, m_file, m_context);
    }
    switch(gpr) {
        case GP_OK: {
            break;
        }
        case GP_ERROR_FILE_NOT_FOUND:
        case GP_ERROR_DIRECTORY_NOT_FOUND: {
            gp_file_unref(m_file);
            m_file = NULL;
            error(KIO::ERR_DOES_NOT_EXIST, url.fileName());
            return;
        }
        default: {
            gp_file_unref(m_file);
            m_file = NULL;
            error(KIO::ERR_UNKNOWN, QString::fromLocal8Bit(gp_result_as_string(gpr)));
            return;
        }
    }

    // We need to pass left over data here. Some camera drivers do not
    // implement progress callbacks!
    const char *fileData;
    long unsigned int fileSize;
    // This merely returns us a pointer to gphoto's internal data
    // buffer -- there's no expensive memcpy
    gpr = gp_file_get_data_and_size(m_file, &fileData, &fileSize);
    if (gpr != GP_OK) {
        kDebug(7123) << "get():: get_data_and_size failed.";
        gp_file_free(m_file);
        m_file = NULL;
        error(KIO::ERR_UNKNOWN, QString::fromLocal8Bit(gp_result_as_string(gpr)));
        return;
    }
    // make sure we're not sending zero-sized chunks (=EOF)
    // also make sure we send only if the progress did not send the data
    // already.
    if (fileSize > 0 && (fileSize - m_fileSize) > 0) {
        unsigned long written = 0;
        QByteArray chunkDataBuffer;

        // We need to split it up here. Someone considered it funny
        // to discard any data() larger than 16MB.
        //
        // So nearly any Movie will just fail....
        while (written < fileSize-m_fileSize) {
            unsigned long towrite = 1024*1024; // 1MB

            if (towrite > fileSize-m_fileSize-written) {
                towrite = fileSize-m_fileSize-written;
            }
            chunkDataBuffer = QByteArray::fromRawData(fileData + m_fileSize + written, towrite);
            processedSize(m_fileSize + written + towrite);
            data(chunkDataBuffer);
            chunkDataBuffer.clear();
            written += towrite;
        }
        m_fileSize = fileSize;
        setFileSize(fileSize);
    }

    finished();
    gp_file_unref(m_file); /* just unref, might be stored in fs */
    m_file = NULL;
}

// The KIO slave "stat" function.
void KameraProtocol::stat(const KUrl &url)
{
    kDebug(7123) << "stat(\"" << url.path() << "\")";

    if (url.path().isEmpty()) {
        KUrl rooturl(url);

        kDebug(7123) << "redirecting to /";
        rooturl.setPath("/");
        redirection(rooturl);
        finished();
        return;
    }
    if (url.path() == "/") {
        statRoot();
    } else {
        statRegular(url);
    }
}

// Implements stat("/") -- which always returns the same value.
void KameraProtocol::statRoot(void)
{
    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, QString::fromLatin1("/"));
    entry.insert(KIO::UDSEntry::UDS_URL, "camera:/");
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, (S_IRUSR | S_IRGRP | S_IROTH));
    entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, QString::fromLatin1("inode/directory"));
    statEntry(entry);
    finished();
    // If we just do this call, timeout right away if no other requests are
    // pending. This is for the kdemm autodetection using media://camera
    idletime = MAXIDLETIME;
}

void KameraProtocol::split_url2camerapath(const QString &url, QString &directory, QString &file) {
    QStringList components, camarr;
    QString cam, camera, port;

    components = url.split('/', QString::SkipEmptyParts);
    if (components.size() == 0) {
        return;
    }
    cam = path_unquote(components.takeFirst());
    if (!cam.isEmpty()) {
        camarr = cam.split('@');
        camera = path_unquote(camarr.takeFirst());
        port = path_unquote(camarr.takeLast());
        setCamera (camera, port);
    }
    if (components.size() == 0)  {
        directory = "/";
        return;
    }
    file = path_unquote(components.takeLast());
    directory = path_unquote("/"+components.join("/"));
}

// Implements a regular stat() of a file / directory, returning all we know about it
void KameraProtocol::statRegular(const KUrl &xurl)
{
    KIO::UDSEntry entry;
    QString directory, file;
    int gpr;

    kDebug(7123) << "statRegular(\"" << xurl.path() << "\")";

    split_url2camerapath(xurl.path(), directory, file);

    if (openCamera() == false) {
        error(KIO::ERR_DOES_NOT_EXIST, xurl.path());
        return;
    }

    if (directory == "/") {
        KIO::UDSEntry entry;

        QString xname = current_camera + "@" + current_port;
        entry.insert(KIO::UDSEntry::UDS_NAME, path_quote(xname));
        entry.insert(KIO::UDSEntry::UDS_URL, xurl.url());
        entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, current_camera);
        entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.insert(KIO::UDSEntry::UDS_ACCESS, (S_IRUSR | S_IRGRP | S_IROTH));
        entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, QString::fromLatin1("inode/directory"));
        statEntry(entry);
        finished();
        return;
    }

    // Is "url" a directory?
    CameraList *dirList;
    gp_list_new(&dirList);
    kDebug(7123) << "statRegular() Requesting directories list for " << directory;

    gpr = gp_camera_folder_list_folders(m_camera, tocstr(fix_foldername(directory)), dirList, m_context);
    if (gpr != GP_OK) {
        if (gpr == GP_ERROR_FILE_NOT_FOUND || gpr == GP_ERROR_DIRECTORY_NOT_FOUND) {
            error(KIO::ERR_DOES_NOT_EXIST, xurl.path());
        } else {
            error(KIO::ERR_UNKNOWN, QString::fromLocal8Bit(gp_result_as_string(gpr)));
        }
        gp_list_free(dirList);
        return;
    }

#define GPHOTO_TEXT_FILE(xx)                                    \
    if (directory == "/" && file == #xx".txt") {                \
        CameraText xx;                                          \
        gpr = gp_camera_get_##xx(m_camera, &xx, m_context);     \
        if (gpr != GP_OK) {                                     \
            error(KIO::ERR_DOES_NOT_EXIST, xurl.fileName());    \
            return;                                             \
        }                                                       \
        translateTextToUDS(entry, #xx".txt", xx.text);          \
        statEntry(entry);                                       \
        finished();                                             \
        return;                                                 \
    }
    GPHOTO_TEXT_FILE(about);
    GPHOTO_TEXT_FILE(manual);
    GPHOTO_TEXT_FILE(summary);
#undef GPHOTO_TEXT_FILE

    const char *name;
    for(int i = 0; i < gp_list_count(dirList); i++) {
        gp_list_get_name(dirList, i, &name);
        if (file == name) {
            gp_list_free(dirList);
            KIO::UDSEntry entry;
            translateDirectoryToUDS(entry, file);
            statEntry(entry);
            finished();
            return;
        }
    }
    gp_list_free(dirList);

    // Is "url" a file?
    CameraFileInfo info;
    gpr = gp_camera_file_get_info(m_camera, tocstr(fix_foldername(directory)), tocstr(file), &info, m_context);
    if (gpr != GP_OK) {
        if (gpr == GP_ERROR_FILE_NOT_FOUND || gpr == GP_ERROR_DIRECTORY_NOT_FOUND) {
            error(KIO::ERR_DOES_NOT_EXIST, xurl.path());
        } else {
            error(KIO::ERR_UNKNOWN, QString::fromLocal8Bit(gp_result_as_string(gpr)));
        }
        return;
    }
    translateFileToUDS(entry, info, file);
    statEntry(entry);
    finished();
}

// The KIO slave "del" function.
void KameraProtocol::del(const KUrl &url, bool isFile)
{
    QString directory, file;
    kDebug(7123) << "KameraProtocol::del(" << url.path() << ")";

    split_url2camerapath(url.path(), directory, file);
    if (!openCamera()) {
        error(KIO::ERR_CANNOT_DELETE, file);
        return;
    }
    if (!cameraSupportsDel()) {
        error(KIO::ERR_CANNOT_DELETE, file);
        return;
    }
    if (isFile){
        CameraList *list;
        gp_list_new(&list);
        int gpr;

        gpr = gp_camera_file_delete(m_camera, tocstr(fix_foldername(directory)), tocstr(file), m_context);
        if(gpr != GP_OK) {
            error(KIO::ERR_CANNOT_DELETE, file);
        } else {
            finished();
        }
    }
}

// The KIO slave "listDir" function.
void KameraProtocol::listDir(const KUrl &xurl)
{
    QString directory, file;
    kDebug(7123) << "KameraProtocol::listDir(" << xurl.path() << ")";

    split_url2camerapath(xurl.path(), directory, file);

    if (!file.isEmpty()) {
        if (directory == "/") {
            directory = "/" + file;
        } else {
            directory = directory + "/" + file;
        }
    }

    if (xurl.path() == "/") {
        // List the available cameras
        QStringList groupList = m_config->groupList();
        kDebug(7123) << "Found cameras: " << groupList.join(", ");
        QStringList::Iterator it;
        KIO::UDSEntry entry;

        /*
         * What we do:
         * - Autodetect cameras and remember them with their ports.
         * - List all saved and possible offline cameras.
         * - List all autodetected and not yet printed cameras.
         */
        QMap<QString,QString> ports, names;
        QMap<QString,int> modelcnt;

        /* Autodetect USB cameras ... */
        GPContext *glob_context = NULL;
        int i, count;
        CameraList *list;
        CameraAbilitiesList *al;
        GPPortInfoList *il;

        gp_list_new(&list);
        gp_abilities_list_new(&al);
        gp_abilities_list_load(al, glob_context);
        gp_port_info_list_new(&il);
        gp_port_info_list_load(il);
        gp_abilities_list_detect(al, il, list, glob_context);
        gp_abilities_list_free(al);
        gp_port_info_list_free(il);

        count = gp_list_count(list);

        for (i = 0 ; i<count ; i++) {
            const char *model, *value;

            gp_list_get_name(list, i, &model);
            gp_list_get_value(list, i, &value);

            ports[value] = model;
            // NOTE: We might get different ports than usb: later!
            if (strcmp(value,"usb:") != 0) {
                names[model] = value;
            }

            /* Save them, even though we can autodetect them for
             * offline listing.
             */
#if 0
            KConfigGroup cg(m_config, model);
            cg.writeEntry("Model", model);
            cg.writeEntry("Path", value);
#endif
            modelcnt[model]++;
        }
        gp_list_free(list);

        /* Avoid duplicated entry, that is a camera with both port usb: and usb:001,042 entries. */
        if (ports.contains("usb:") && names.contains(ports["usb:"]) && names[ports["usb:"]] != "usb:") {
            ports.remove("usb:");
        }

        for (it = groupList.begin(); it != groupList.end(); it++) {
            QString m_cfgPath;
            if (*it == "<default>") {
                continue;
            }

            KConfigGroup cg(m_config, *it);
            m_cfgPath = cg.readEntry("Path");

            // we autodetected those ...
            if (m_cfgPath.contains(QString("usb:"))) {
                cg.deleteGroup();
                continue;
            }

            QString xname;

            entry.clear();
            entry.insert(KIO::UDSEntry::UDS_FILE_TYPE,S_IFDIR);
            entry.insert(KIO::UDSEntry::UDS_ACCESS,(S_IRUSR | S_IRGRP | S_IROTH |S_IWUSR | S_IWGRP | S_IWOTH));
            xname = (*it) + "@" + m_cfgPath;
            entry.insert(KIO::UDSEntry::UDS_NAME, path_quote(xname));
            // TODO: entry.insert(KIO::UDSEntry::UDS_URL, xurl.url());
            // do not confuse regular users with the @usb... 
            entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, *it);
            listEntry(entry, false);
        }

        QMap<QString,QString>::iterator portsit;
        for (portsit = ports.begin(); portsit != ports.end(); portsit++) {
            entry.clear();
            entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
            // do not confuse regular users with the @usb... 
            entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, portsit.value());
            entry.insert(KIO::UDSEntry::UDS_NAME, path_quote(portsit.value() + "@" + portsit.key()));
            // TODO: entry.insert(KIO::UDSEntry::UDS_URL, xurl.url());

            entry.insert(KIO::UDSEntry::UDS_ACCESS,(S_IRUSR | S_IRGRP | S_IROTH |S_IWUSR | S_IWGRP | S_IWOTH));
            listEntry(entry, false);
        }
        listEntry(entry, true);

        finished();
        return;
    }

    if (directory.isEmpty()) {
        KUrl rooturl(xurl);

        kDebug(7123) << "redirecting to /";
        if (!current_camera.isEmpty() && !current_port.isEmpty()) {
            rooturl.setPath("/" + current_camera + "@" + current_port + "/");
        } else {
            rooturl.setPath("/");
        }
        redirection(rooturl);
        finished();
        return;
    }

    if (!openCamera()) {
        error(KIO::ERR_COULD_NOT_READ, xurl.path());
        return;
    }

    CameraList *dirList;
    CameraList *fileList;
    CameraList *specialList;
    gp_list_new(&dirList);
    gp_list_new(&fileList);
    gp_list_new(&specialList);
    int gpr;

    if (directory != "/") {
        CameraText text;
        if (gp_camera_get_manual(m_camera, &text, m_context) == GP_OK) {
            gp_list_append(specialList,"manual.txt",NULL);
        }
        if (gp_camera_get_about(m_camera, &text, m_context) == GP_OK) {
            gp_list_append(specialList,"about.txt",NULL);
        }
        if (gp_camera_get_summary(m_camera, &text, m_context) == GP_OK) {
            gp_list_append(specialList,"summary.txt",NULL);
        }
    }

    gpr = readCameraFolder(directory, dirList, fileList);
    if (gpr != GP_OK) {
        kDebug(7123) << "read Camera Folder failed:" << gp_result_as_string(gpr);
        gp_list_free(dirList);
        gp_list_free(fileList);
        gp_list_free(specialList);
        error(KIO::ERR_SLAVE_DEFINED, i18n("Could not read. Reason: %1", QString::fromLocal8Bit(gp_result_as_string(gpr))));
        return;
    }

    totalSize(gp_list_count(specialList) + gp_list_count(dirList) + gp_list_count(fileList));

    KIO::UDSEntry entry;
    const char *name;

    for(int i = 0; i < gp_list_count(dirList); ++i) {
        gp_list_get_name(dirList, i, &name);
        translateDirectoryToUDS(entry, QString::fromLocal8Bit(name));
        listEntry(entry, false);
    }

    CameraFileInfo info;

    for(int i = 0; i < gp_list_count(fileList); ++i) {
        gp_list_get_name(fileList, i, &name);
        // we want to know more info about files (size, type...)
        gp_camera_file_get_info(m_camera, tocstr(directory), name, &info, m_context);
        translateFileToUDS(entry, info, QString::fromLocal8Bit(name));
        listEntry(entry, false);
    }
    if (directory != "/") {
        CameraText text;
        if (gp_camera_get_manual(m_camera, &text, m_context) == GP_OK) {
            translateTextToUDS(entry, "manual.txt", text.text);
            listEntry(entry, false);
        }
        if (gp_camera_get_about(m_camera, &text, m_context) == GP_OK) {
            translateTextToUDS(entry, "about.txt", text.text);
            listEntry(entry, false);
        }
        if (gp_camera_get_summary(m_camera, &text, m_context) == GP_OK) {
            translateTextToUDS(entry, "summary.txt", text.text);
            listEntry(entry, false);
        }
    }


    gp_list_free(fileList);
    gp_list_free(dirList);
    gp_list_free(specialList);

    listEntry(entry, true); // 'entry' is not used in this case - we only signal list completion
    finished();
}

void KameraProtocol::setCamera(const QString& camera, const QString& port)
{
    kDebug(7123) << "KameraProtocol::setCamera(" << camera << ", " << port << ")";
    int gpr, idx;

    if (!camera.isEmpty() && !port.isEmpty()) {
        kDebug(7123) << "model is " << camera << ", port is " << port;

        if (m_camera && current_camera == camera && current_port == port) {
            kDebug(7123) << "Configuration is same, nothing to do.";
            return;
        }
        if (m_camera) {
            kDebug(7123) << "Configuration change detected";
            closeCamera();
            gp_camera_unref(m_camera);
            m_camera = NULL;
            infoMessage(i18n("Reinitializing camera"));
        } else {
            kDebug(7123) << "Initializing camera";
            infoMessage(i18n("Initializing camera"));
        }
        // fetch abilities
        CameraAbilitiesList *abilities_list;
        gp_abilities_list_new(&abilities_list);
        gp_abilities_list_load(abilities_list, m_context);
        idx = gp_abilities_list_lookup_model(abilities_list, tocstr(camera));
        if (idx < 0) {
            gp_abilities_list_free(abilities_list);
            kDebug(7123) << "Unable to get abilities for model: " << camera;
            error(KIO::ERR_UNKNOWN, QString::fromLocal8Bit(gp_result_as_string(idx)));
            return;
        }
        gp_abilities_list_get_abilities(abilities_list, idx, &m_abilities);
        gp_abilities_list_free(abilities_list);

        // fetch port
        GPPortInfoList *port_info_list;
        GPPortInfo port_info;
        gp_port_info_list_new(&port_info_list);
        gp_port_info_list_load(port_info_list);
        idx = gp_port_info_list_lookup_path(port_info_list, tocstr(port));

        /* Handle erronously passed usb:XXX,YYY */
        if (idx < 0 && port.startsWith("usb:")) {
            idx = gp_port_info_list_lookup_path(port_info_list, "usb:");
        }
        if (idx < 0) {
            gp_port_info_list_free(port_info_list);
            kDebug(7123) << "Unable to get port info for path: " << port;
            error(KIO::ERR_UNKNOWN, QString::fromLocal8Bit(gp_result_as_string(idx)));
            return;
        }
        gp_port_info_list_get_info(port_info_list, idx, &port_info);

        current_camera	= camera;
        current_port	= port;
        // create a new camera object
        gpr = gp_camera_new(&m_camera);
        if (gpr != GP_OK) {
            gp_port_info_list_free(port_info_list);
            error(KIO::ERR_UNKNOWN, QString::fromLocal8Bit(gp_result_as_string(gpr)));
            return;
        }

        // register gphoto2 callback functions
        gp_context_set_status_func(m_context, frontendCameraStatus, this);
        gp_context_set_progress_funcs(m_context, frontendProgressStart, frontendProgressUpdate, NULL, this);
        // gp_camera_set_message_func(m_camera, ..., this)

        // set model and port
        gp_camera_set_abilities(m_camera, m_abilities);
        gp_camera_set_port_info(m_camera, port_info);
        gp_camera_set_port_speed(m_camera, 0); // TODO: the value needs to be configurable
        kDebug(7123) << "Opening camera model " << camera << " at " << port;

        gp_port_info_list_free(port_info_list);

        QString errstr;
        if (!openCamera(errstr)) {
            if (m_camera) {
                gp_camera_unref(m_camera);
            }
            m_camera = NULL;
            kDebug(7123) << "Unable to init camera: " << errstr;
            error(KIO::ERR_SERVICE_NOT_AVAILABLE, errstr);
            return;
        }
    }
}

void KameraProtocol::reparseConfiguration(void)
{
    // we have no global config, do we?
}

// translate a simple text to a UDS entry
void KameraProtocol::translateTextToUDS(KIO::UDSEntry &udsEntry, const QString &fn, const char *text)
{

    udsEntry.clear();

    udsEntry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    udsEntry.insert(KIO::UDSEntry::UDS_NAME, path_quote(fn));
    // TODO: udsEntry.insert(KIO::UDSEntry::UDS_URL, path_quote(fn));
    udsEntry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, fn);
    udsEntry.insert(KIO::UDSEntry::UDS_SIZE,strlen(text));
    udsEntry.insert(KIO::UDSEntry::UDS_ACCESS,(S_IRUSR | S_IRGRP | S_IROTH));
}

// translate a CameraFileInfo to a UDSFieldType which we can return as a directory listing entry
void KameraProtocol::translateFileToUDS(KIO::UDSEntry &udsEntry, const CameraFileInfo &info, QString name)
{

    udsEntry.clear();

    udsEntry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    udsEntry.insert(KIO::UDSEntry::UDS_NAME, path_quote(name));
    // TODO: udsEntry.insert(KIO::UDSEntry::UDS_URL, path_quote(fn));
    udsEntry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, name);

    if (info.file.fields & GP_FILE_INFO_SIZE) {
        udsEntry.insert(KIO::UDSEntry::UDS_SIZE,info.file.size);
    }

    if (info.file.fields & GP_FILE_INFO_MTIME) {
        udsEntry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, info.file.mtime);
    } else {
        udsEntry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, time(NULL));
    }

    if (info.file.fields & GP_FILE_INFO_TYPE) {
        udsEntry.insert(KIO::UDSEntry::UDS_MIME_TYPE, QString::fromLatin1(info.file.type));
    }

    if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
        udsEntry.insert(KIO::UDSEntry::UDS_ACCESS,((info.file.permissions & GP_FILE_PERM_READ) ? (S_IRUSR | S_IRGRP | S_IROTH) : 0));
    } else {
        udsEntry.insert(KIO::UDSEntry::UDS_ACCESS,S_IRUSR | S_IRGRP | S_IROTH);
    }

    // TODO: We do not handle info.preview in any way
}

// translate a directory name to a UDSFieldType which we can return as a directory listing entry
void KameraProtocol::translateDirectoryToUDS(KIO::UDSEntry &udsEntry, const QString &dirname)
{

    udsEntry.clear();

    udsEntry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    udsEntry.insert(KIO::UDSEntry::UDS_NAME, path_quote(dirname));
    // TODO: udsEntry.insert(KIO::UDSEntry::UDS_URL, path_quote(dirname));
    udsEntry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, dirname);
    udsEntry.insert(KIO::UDSEntry::UDS_ACCESS,S_IRUSR | S_IRGRP | S_IROTH |S_IWUSR | S_IWGRP | S_IWOTH | S_IXUSR | S_IXOTH | S_IXGRP);
    udsEntry.insert(KIO::UDSEntry::UDS_MIME_TYPE, QString("inode/directory"));
}

bool KameraProtocol::cameraSupportsDel(void)
{
    return (m_abilities.file_operations & GP_FILE_OPERATION_DELETE);
}

bool KameraProtocol::cameraSupportsPut(void)
{
    return (m_abilities.folder_operations & GP_FOLDER_OPERATION_PUT_FILE);
}

bool KameraProtocol::cameraSupportsPreview(void)
{
    return (m_abilities.file_operations & GP_FILE_OPERATION_PREVIEW);
}

int KameraProtocol::readCameraFolder(const QString &folder, CameraList *dirList, CameraList *fileList)
{
    kDebug(7123) << "KameraProtocol::readCameraFolder(" << folder << ")";

    int gpr = gpr = gp_camera_folder_list_folders(m_camera, tocstr(folder), dirList, m_context);
    if (gpr != GP_OK) {
        return gpr;
    }
    gpr = gp_camera_folder_list_files(m_camera, tocstr(folder), fileList, m_context);
    if (gpr != GP_OK) {
        return gpr;
    }
    return GP_OK;
}

void frontendProgressUpdate(GPContext * /*context*/, unsigned int /*id*/, float /*current*/, void *data)
{
    KameraProtocol *object = (KameraProtocol*)data;

    // This code will get the last chunk of data retrieved from the
    // camera and pass it to KIO, to allow progressive display
    // of the downloaded photo.

    const char *fileData = NULL;
    long unsigned int fileSize = 0;

    // This merely returns us a pointer to gphoto's internal data
    // buffer -- there's no expensive memcpy
    if (!object->getFile()) {
        return;
    }
    gp_file_get_data_and_size(object->getFile(), &fileData, &fileSize);
    // make sure we're not sending zero-sized chunks (=EOF)
    if (fileSize > 0) {
        // XXX using assign() here causes segfault, prolly because
        // gp_file_free is called before chunkData goes out of scope
        QByteArray chunkDataBuffer = QByteArray::fromRawData(fileData + object->getFileSize(), fileSize - object->getFileSize());
        // Note: this will fail with sizes > 16MB ...
        object->data(chunkDataBuffer);
        object->processedSize(fileSize);
        chunkDataBuffer.clear();
        object->setFileSize(fileSize);
    }
}

unsigned int frontendProgressStart(GPContext * /*context*/, float totalsize, const char *status, void *data)
{
    KameraProtocol *object = (KameraProtocol*)data;
    object->infoMessage(QString::fromLocal8Bit(status));
    object->totalSize((KIO::filesize_t)totalsize); // hack: call slot directly
    return GP_OK;
}

// this callback function is activated on every status message from gphoto2
static void frontendCameraStatus(GPContext * /*context*/, const char *status, void *data)
{
    KameraProtocol *object = (KameraProtocol*)data;
    object->infoMessage(QString::fromLocal8Bit(status));
}
