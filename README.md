# LLM Text RPG

중세 판타지 텍스트 RPG 프로토타입입니다. LLM은 장면과 이벤트 후보만 만들고, 전투 수치는 C++ 코드가 처리합니다.

## 구조

- `main.cpp`: 앱 진입점. 설정을 만든 뒤 `TextRpgApp` 실행
- `src/app/TextRpgApp.*`: 콘솔 게임 루프, 입력/출력, 거점 확인, 장로 대화, 기본 전투 선택 처리
- `src/llm/LLM.*`: public LLM 엔진 facade. 호출자는 결과 객체만 받고 raw JSON은 다루지 않음
- `src/llm/OllamaChatClient.*`: Ollama SDK 어댑터
- `src/llm/PromptBuilder.*`: `GameState`와 행동 컨텍스트를 LLM 프롬프트로 변환
- `src/llm/RagContext.*`: Ollama embedding RAG. 실패하거나 `TEXTRPG_RAG=off`면 키워드 검색으로 fallback
- `src/llm/RecordStore.*`: JSON 기록 로드/저장
- `src/llm/ResponseParser.*`: JSON 추출, 객체 매핑, 검증/fallback
- `src/llm/StateApplier.*`: 파싱된 객체를 `GameState`에 반영
- `src/llm/LLMTypes.hpp`: 게임 상태와 LLM 이벤트 타입
- `src/combat/CombatSystem.*`: 단순 전투 처리
- `src/combat/CombatTypes.hpp`: 전투 타입
- `data/game_state.json`: 시작 플레이어, 시작 아이템, 세계 기본 규칙
- `data/config.json`: Ollama 모델, endpoint, 게임 데이터/기록 경로 설정
- `data/game_records.example.json`: 런타임 기록 JSON 예시

## 현재 규칙

- LLM 백엔드: Ollama 전용
- 기본 Ollama 모델: `aravhawk/gemma4:26b`
- 시작 위치, 첫 장면, 첫 목표, 첫 판단 기준은 LLM이 생성
- 게임 시작 시 주인공 입장의 프롤로그를 한 번 생성하고 JSON 기록에 저장
- 프롤로그는 주인공의 결핍/상처, 개인 목표, 시작 위치, 첫 목표를 정하고 이후 프롬프트에 반영
- 매 턴 위험도가 랜덤으로 증가하고, 위험도가 10 이상이면 C++ 엔진이 전투 이벤트를 강제
  - 위험도 증가는 한 턴에 1~10이며, 큰 증가량일수록 나올 확률이 낮음
  - 전투가 발생하면 위험도는 0으로 리셋
  - 위험도가 10 미만이면 LLM의 `combat` 선택은 차단됨
- 전투가 아닌 다음 이벤트 유형은 LLM이 현재 목표, 판단 기준, 최근 사건, 직전 행동을 보고 선택
  - 가능한 이벤트: `story`, `combat`, `item_gain`, `stat_change`, `dialogue`, `quest_update`, `rest`, `game_end`
  - LLM 엔진은 LLM 응답을 파싱/검증하고 `GameState`에 위치, 목표, 상태 변화, 아이템, 메모리를 반영
  - 전투 HP 계산은 `CombatSystem`이 담당하므로 `combat` 이벤트의 LLM HP 변화는 적용하지 않음
- 고유 행동은 `ActionResult`로 직접 결과를 먼저 확정한 뒤, 그 결과를 컨텍스트로 다시 다음 `GameEvent`를 생성
- 고유 행동에 `거점`이 들어가거나 현재 지역이 마을/도시/여관/시장처럼 안전 거점 후보면 예/아니오 확인 후 거점 기능 해방
- 거점 기능은 무기 거래, 무기 강화, 여관, 아이템 구매를 해방 목록으로 저장
- 거점에 있으면 장로와 한 번만 대화할 수 있고, 이때 LLM이 보스 이름/위치/약점/설명을 생성
- 장로 대화 완료 여부와 보스 정보는 C++이 확정해 JSON 기록에 저장
- 퀘스트 기록, 획득 아이템 기록, 조우한 적 기록은 `GameState.records`와 JSON 파일에 저장
- 프롤로그, 거점 해방 상태, 거점 거절 지역, 장로 대화 상태, 보스 정보도 JSON 파일에 저장
- 시작 플레이어, 시작 아이템, 세계 기본 규칙은 `data/game_state.json`에서 로드
- 기본 기록 JSON 경로: `data/game_records.json`
  - 이 파일은 실행 중 자동 생성되며 git에는 올리지 않음
  - 다른 경로를 쓰려면 `data/config.json`의 `records_json_path`를 수정
- RAG 컨텍스트는 JSON 기록과 현재 메모리를 Ollama embedding으로 검색해 프롬프트에 추가
  - 기본 embedding 모델: `nomic-embed-text`
  - `TEXTRPG_RAG_MODEL`, `TEXTRPG_RAG_ENDPOINT`로 설정 가능
  - Ollama embedding 호출 실패 또는 `TEXTRPG_RAG=off`면 키워드 검색 fallback
- 플레이어: HP 999, 공격력 9
- 몬스터: LLM이 제안한 HP/공격/방어를 C++에서 안전 범위로 보정해 사용
- 전투 기본 선택지: 공격, 아이템
- 비전투는 직접 문장 입력. 단, 거점/장로처럼 엔진이 처리하는 행동은 번호 선택지로 표시됨
- 고유 행동은 d12로 판정
  - 1~5: 실패
  - 6~10: 성공
  - 11~12: 초대박
- 초대박은 아이템 직접 획득으로 보상하지 않고 단서, 위치, 전황, 퀘스트 진전, 위험 회피로 처리
- 전투 계산은 현재 `때리기`만 사용
- `weapon`/`armor`는 획득 시 공격/방어에 반영
- `consumable`은 전투 중 아이템 선택으로 HP 회복에 사용
- `quest_item`은 진행용 물건이며 수치 효과와 value가 0으로 보정됨
- 이미 기록된 아이템은 같은 이름으로 다시 등장해도 type/description/value를 기존 기록 기준으로 유지

## LLM 타입 확장

LLM JSON과 맞물리는 타입은 enum 대신 문자열 ID를 사용합니다. 새 ID를 추가할 때는 `src/llm/LLMTypes.hpp`의 `ids` 목록과 프롬프트/repair 정책, 테스트를 함께 갱신하세요.

- 이벤트 ID: `story`, `combat`, `item_gain`, `stat_change`, `dialogue`, `quest_update`, `rest`, `game_end`
- 아이템 ID: `weapon`, `armor`, `consumable`, `quest_item`
- d12 결과 ID: `failure`, `success`, `jackpot`

새 이벤트 타입을 추가할 때:

1. `ids::event`와 `eventTypeIds()`에 ID를 추가합니다.
2. `PromptBuilder`의 다음 장면 프롬프트 선택 규칙을 추가합니다.
3. 상태 반영이 필요하면 `StateApplier`의 event apply 정책을 갱신합니다.
4. 파싱/보정 정책을 갱신합니다.

## 빌드

Ollama 전용 빌드입니다. 외부 API 키는 필요 없습니다.

필요한 것:

- Visual Studio Build Tools 또는 Visual Studio의 C++ 빌드 도구
- CMake 3.20 이상
- PowerShell

1. 저장소 폴더로 이동합니다.

```powershell
cd C:\Users\chase\Documents\GitHub\TextRPG-LLM
```

2. CMake configure를 실행합니다.

```powershell
cmake -S . -B build-vcpkg
```

3. Debug 빌드를 실행합니다.

```powershell
cmake --build build-vcpkg --config Debug
```

성공하면 실행 파일이 여기에 생깁니다.

```text
build-vcpkg\Debug\llm_text_rpg.exe
```

4. Release 빌드가 필요하면 이렇게 빌드합니다.

```powershell
cmake --build build-vcpkg --config Release
```

Release 실행 파일 위치:

```text
build-vcpkg\Release\llm_text_rpg.exe
```

5. 빌드 결과를 지우고 처음부터 다시 구성하고 싶으면 `build-vcpkg` 폴더를 삭제한 뒤 configure부터 다시 실행합니다.

```powershell
Remove-Item -Recurse -Force build-vcpkg
```

자주 나는 문제:

- `cl.exe` 또는 C++ 컴파일러를 찾지 못함  
  Visual Studio C++ 빌드 도구가 설치되어 있는지 확인하세요.
- Ollama 연결 실패  
  Ollama 서버가 실행 중인지, `data/config.json`의 endpoint가 맞는지 확인하세요.

## 실행

Ollama 서버를 실행하고 필요한 모델을 받아둡니다.

```powershell
ollama serve
ollama pull aravhawk/gemma4:26b
ollama pull nomic-embed-text
```

실행 설정은 `data/config.json`에서 관리합니다.

```json
{
  "llm": {
    "provider": "ollama",
    "endpoint": "http://localhost:11434",
    "model": "aravhawk/gemma4:26b",
    "temperature": 0.7,
    "think": false,
    "connection_timeout_seconds": 10,
    "read_timeout_seconds": 300
  },
  "game_data_path": "data/game_state.json",
  "records_json_path": "data/game_records.json",
  "scripted_inputs": []
}
```

Release 실행:

```powershell
.\build-vcpkg\Release\llm_text_rpg.exe
```
