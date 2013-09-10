#include <QtGui>
#include <QHeaderView>
#include <QPainter>

#include "QtAwesome.h"
#include "utils/utils.h"
#include "seafile-applet.h"
#include "rpc/rpc-client.h"
#include "rpc/local-repo.h"
#include "sync-repo-dialog.h"
#include "repo-item.h"
#include "repo-tree-model.h"
#include "repo-tree-view.h"

namespace {

const int kSyncStatusIconSize = 16;

}

RepoTreeView::RepoTreeView(QWidget *parent)
    : QTreeView(parent)
{
    header()->hide();
    createContextMenu();
}

void RepoTreeView::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint pos = event->pos();
    QModelIndex index = indexAt(pos);
    if (!index.isValid()) {
        // Not clicked at a repo item
        return;
    }

    RepoItem *item = getRepoItem(index);
    if (!item) {
        return;
    }

    prepareContextMenu(item->repo());
    pos = viewport()->mapToGlobal(pos);
    context_menu_->exec(pos);
}

void RepoTreeView::prepareContextMenu(const ServerRepo& repo)
{
    qDebug("repo id is %s\n", toCStr(repo.id));

    show_detail_action_->setData(QVariant::fromValue(repo));
    open_local_folder_action_->setData(QVariant::fromValue(repo));
    download_action_->setData(QVariant::fromValue(repo));

    if (seafApplet->rpcClient()->hasLocalRepo(repo.id)) {
        download_action_->setVisible(false);
        open_local_folder_action_->setVisible(true);
    } else {
        download_action_->setVisible(true);
        open_local_folder_action_->setVisible(false);
    }

}

void RepoTreeView::drawBranches(QPainter *painter,
                                const QRect& rect,
                                const QModelIndex& index) const
{
    RepoItem *item = getRepoItem(index);
    if (!item) {
        QTreeView::drawBranches(painter, rect, index);
        return;
    }

    painter->save();
    painter->setFont(awesome->font(kSyncStatusIconSize));
    painter->drawText(rect,
                      Qt::AlignCenter | Qt::AlignVCenter,
                      getSyncStatusIcon(item->repo()));
    painter->restore();
}

QChar RepoTreeView::getSyncStatusIcon(const ServerRepo& repo) const
{
    LocalRepo local_repo;
    if (seafApplet->rpcClient()->getLocalRepo(repo.id, &local_repo) < 0) {
        // No local repo, return a cloud icon to indicate this
        return icon_cloud;
    } else {
        // Has local repo, return an icon according to sync status
        switch (local_repo.sync_state) {
        case LocalRepo::SYNC_STATE_DONE:
            return icon_ok;
        case LocalRepo::SYNC_STATE_ING:
            return icon_refresh;
        case LocalRepo::SYNC_STATE_ERROR:
            return icon_exclamation;
        case LocalRepo::SYNC_STATE_WAITING:
            return icon_pause;
        case LocalRepo::SYNC_STATE_DISABLED:
            return icon_minus_sign;
        case LocalRepo::SYNC_STATE_UNKNOWN:
            return icon_question_sign;
        }
    }
}

RepoItem* RepoTreeView::getRepoItem(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return NULL;
    }
    const RepoTreeModel *model = (const RepoTreeModel*)index.model();
    QStandardItem *item = model->itemFromIndex(index);
    if (item->type() != REPO_ITEM_TYPE) {
        // qDebug("drawBranches: %s\n", item->data(Qt::DisplayRole).toString().toUtf8().data());
        return NULL;
    }
    return (RepoItem *)item;
}

void RepoTreeView::createContextMenu()
{
    context_menu_ = new QMenu(this);

    show_detail_action_ = new QAction(tr("&Show details"), this);
    show_detail_action_->setIcon(awesome->icon(icon_info_sign));
    show_detail_action_->setStatusTip(tr("Download this library"));
    connect(show_detail_action_, SIGNAL(triggered()), this, SLOT(showRepoDetail()));

    // context_menu_->addAction(show_detail_action_);

    download_action_ = new QAction(tr("&Download this library"), this);
    download_action_->setIcon(awesome->icon(icon_download));
    download_action_->setStatusTip(tr("Download this library"));
    connect(download_action_, SIGNAL(triggered()), this, SLOT(downloadRepo()));

    context_menu_->addAction(download_action_);

    open_local_folder_action_ = new QAction(tr("&Open folder"), this);
    open_local_folder_action_->setIcon(awesome->icon(icon_folder_open_alt));
    open_local_folder_action_->setStatusTip(tr("open this folder with file manager"));
    connect(open_local_folder_action_, SIGNAL(triggered()), this, SLOT(openLocalFolder()));

    context_menu_->addAction(open_local_folder_action_);
}

void RepoTreeView::downloadRepo()
{
    ServerRepo repo = qvariant_cast<ServerRepo>(download_action_->data());
    SyncRepoDialog dialog(repo, this);
    dialog.exec();
}

void RepoTreeView::showRepoDetail()
{
}

void RepoTreeView::openLocalFolder()
{
    ServerRepo repo = qvariant_cast<ServerRepo>(download_action_->data());

    LocalRepo local_repo;
    if (seafApplet->rpcClient()->getLocalRepo(toCStr(repo.id), &local_repo) < 0) {
        return;
    }

    open_dir(local_repo.worktree);
}
