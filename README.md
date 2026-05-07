# LLM Text RPG

중세 판타지 텍스트 RPG 프로토타입입니다. LLM은 장면과 이벤트 후보만 만들고, 전투 수치는 C++ 코드가 처리합니다.

## 구조

- `main.cpp`: 데모 게임 루프
- `src/llm/LLM.*`: public LLM 엔진 facade. 호출자는 결과 객체만 받고 raw JSON은 다루지 않음
- `src/llm/OpenAIChatClient.*`: OpenAI Chat Completions API 어댑터
- `src/llm/OllamaChatClient.*`: Ollama SDK 어댑터. `IChatClient` 구현으로 백엔드 교체 가능
- `src/llm/PromptBuilder.*`: `GameState`와 행동 컨텍스트를 LLM 프롬프트로 변환
- `src/llm/ResponseParser.*`: JSON 추출, 객체 매핑, 검증/fallback
- `src/llm/StateApplier.*`: 파싱된 객체를 `GameState`에 반영
- `src/llm/LLMEngineInternals.hpp`: 테스트와 내부 모듈 연결용 얇은 선언
- `src/llm/LLMTypes.hpp`: 게임 상태와 LLM 이벤트 타입
- `src/combat/CombatSystem.*`: 단순 전투 처리
- `src/combat/CombatTypes.hpp`: 전투 타입

## 현재 규칙

- LLM 백엔드 기본값: OpenAI
- 기본 OpenAI 모델: `gpt-4.1-mini`
- Ollama도 `TEXTRPG_LLM_PROVIDER=ollama` 또는 실행 인자 `ollama`로 계속 사용 가능
- 시작 위치, 첫 장면, 첫 목표, 첫 판단 기준은 LLM이 생성
- 매 턴 다음 이벤트 유형은 LLM이 현재 목표, 판단 기준, 최근 사건, 직전 행동을 보고 선택
  - 가능한 이벤트: `story`, `combat`, `item_gain`, `stat_change`, `dialogue`, `quest_update`, `rest`, `game_end`
  - LLM 엔진은 LLM 응답을 파싱/검증하고 `GameState`에 위치, 목표, 상태 변화, 아이템, 메모리를 반영
  - 전투 HP 계산은 `CombatSystem`이 담당하므로 `combat` 이벤트의 LLM HP 변화는 적용하지 않음
- 플레이어: HP 999, 공격력 9
- 몬스터: HP 10, 공격력 1
- 전투 기본 선택지: 공격, 스킬, 아이템, 고유 행동
- 비전투 기본 선택지: 전진, 조사, 고유 행동
- 고유 행동은 d6으로 판정
  - 1~2: 실패
  - 3~4: 성공
  - 5~6: 초대박
- 전투 계산은 현재 `때리기`만 사용
- LLM이 준 몬스터 수치는 무시하고 이름/설명만 사용

## LLM 타입 확장

LLM JSON과 맞물리는 타입은 enum 대신 문자열 ID를 사용합니다. 새 ID를 추가할 때는 `src/llm/LLMTypes.hpp`의 `ids` 목록과 프롬프트/repair 정책, 테스트를 함께 갱신하세요.

- 이벤트 ID: `story`, `combat`, `item_gain`, `stat_change`, `dialogue`, `quest_update`, `rest`, `game_end`
- 아이템 ID: `weapon`, `armor`, `consumable`, `quest_item`
- d6 결과 ID: `failure`, `success`, `jackpot`

새 이벤트 타입을 추가할 때:

1. `ids::event`와 `eventTypeIds()`에 ID를 추가합니다.
2. `PromptBuilder`의 다음 장면 프롬프트 선택 규칙을 추가합니다.
3. 상태 반영이 필요하면 `StateApplier`의 event apply 정책을 갱신합니다.
4. 파싱/보정 테스트를 추가합니다.

## 빌드

OpenAI 백엔드는 `third_party/openai.hpp`를 통해 libcurl을 링크합니다. Windows에서는 vcpkg toolchain을 지정해 빌드하세요.

```powershell
vcpkg install --triplet x64-windows
```

```powershell
cmake -S . -B build-vcpkg -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build-vcpkg
ctest --test-dir build-vcpkg -C Debug --output-on-failure
```

## 실행

OpenAI API 키는 코드나 git에 올리지 말고 환경 변수로 설정합니다.

현재 PowerShell 창에서만 사용할 때:

```powershell
$env:OPENAI_API_KEY="sk-..."
```

Windows 사용자 환경 변수로 저장할 때:

```powershell
setx OPENAI_API_KEY "sk-..."
```

`setx`로 저장한 뒤에는 새 터미널을 열어야 적용됩니다.

```powershell
.\build-vcpkg\Debug\llm_text_rpg.exe
```

모델 지정:

```powershell
.\build-vcpkg\Debug\llm_text_rpg.exe gpt-4.1-mini
```

한 턴 스크립트 실행:

```powershell
.\build-vcpkg\Debug\llm_text_rpg.exe gpt-4.1-mini "숲길의 부서진 표식을 조사한다"
```

Ollama 실행:

```powershell
.\build-vcpkg\Debug\llm_text_rpg.exe ollama llama3.2:latest
```
