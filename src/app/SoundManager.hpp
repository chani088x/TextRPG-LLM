#pragma once

// Windows 전용 사운드 매니저.
// BGM: 별도 스레드에서 PlaySound(SND_SYNC) 반복 — mciSendString의 repeat은 WAV 미지원
// 효과음: PlaySound(SND_ASYNC) — BGM 스레드와 채널이 달라 BGM이 끊기지 않는다
// 비Windows 환경에서는 모든 함수가 아무것도 하지 않는다.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX  // windows.h의 min/max 매크로가 std::max와 충돌하는 것을 방지
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include <atomic>
#include <string>
#include <thread>

namespace textrpg::app {

class SoundManager {
public:
    // 프로세스 전체에서 하나만 유지하는 싱글턴이다.
    static SoundManager& instance()
    {
        static SoundManager inst;
        return inst;
    }

    // BGM을 루프 재생한다.
    // 별도 스레드에서 PlaySound(SND_SYNC)를 반복해 WAV 루프를 구현한다.
    // 이미 재생 중인 BGM이 있으면 먼저 중단하고 새로 시작한다.
    // path: WAV 파일 경로 (예: "data/sound/bgm.wav")
    void playBGM(const std::string& path)
    {
#ifdef _WIN32
        stopBGM(); // 기존 BGM 스레드 정리

        bgmPath_ = path;
        bgmRunning_ = true;

        bgmThread_ = std::thread([this]() {
            while (bgmRunning_) {
                // SND_SYNC: 재생이 끝날 때까지 스레드를 블록한다.
                // bgmRunning_이 false로 바뀌면 PlaySound(NULL)로 중단시키므로
                // 다음 루프 조건 검사에서 탈출한다.
                PlaySoundA(bgmPath_.c_str(), NULL, SND_FILENAME | SND_SYNC);
            }
        });
#else
        (void)path;
#endif
    }

    // BGM을 정지하고 스레드를 종료한다.
    void stopBGM()
    {
#ifdef _WIN32
        if (bgmRunning_) {
            bgmRunning_ = false;
            // 현재 SND_SYNC 블록을 즉시 해제한다.
            PlaySoundA(NULL, NULL, 0);
        }
        if (bgmThread_.joinable()) {
            bgmThread_.join();
        }
#endif
    }

    // 공격 효과음을 비동기로 재생한다.
    void playAttack()
    {
#ifdef _WIN32
        const BOOL played = PlaySoundA(
            "data/sound/attack.wav", NULL,
            SND_FILENAME | SND_ASYNC);
        if (!played) {
            MessageBeep(MB_OK);
        }
#endif
    }

    // 전투 승리 효과음을 비동기로 재생한다.
    void playVictory()
    {
#ifdef _WIN32
        const BOOL played = PlaySoundA(
            "data/sound/victory.wav", NULL,
            SND_FILENAME | SND_ASYNC);
        if (!played) {
            MessageBeep(MB_ICONEXCLAMATION);
        }
#endif
    }

    // 아이템 획득 효과음을 비동기로 재생한다.
    // SND_ASYNC라서 BGM 스레드의 SND_SYNC와 채널이 분리되어 BGM이 끊기지 않는다.
    void playItemGain()
    {
#ifdef _WIN32
        const BOOL played = PlaySoundA(
            "data/sound/item_gain.wav", NULL,
            SND_FILENAME | SND_ASYNC | SND_NOSTOP); // SND_NOSTOP: BGM 스레드 방해 안 함
        if (!played) {
            // WAV 파일이 없을 때 시스템 알림음으로 대체한다.
            MessageBeep(MB_ICONASTERISK);
        }
#endif
    }

    ~SoundManager()
    {
        stopBGM();
    }

private:
    SoundManager() = default;
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

#ifdef _WIN32
    std::thread bgmThread_;          // BGM 루프를 돌리는 전용 스레드
    std::atomic<bool> bgmRunning_ { false }; // 루프 종료 신호
    std::string bgmPath_;            // 현재 재생 중인 BGM 경로
#endif
};

} // namespace textrpg::app
