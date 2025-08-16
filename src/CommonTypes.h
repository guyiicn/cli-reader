#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

enum class SyncStatus { IDLE, IN_PROGRESS, SUCCESS, ERROR };
enum class DeleteScope {
    LocalOnly,      // 删除本地文件，DB记录更新为cloud-only
    CloudOnly,      // 删除云端文件，DB记录更新为local-only
    CloudAndLocal   // 彻底删除
};

#endif // COMMON_TYPES_H
