# LLM Text RPG v2

중세 판타지 텍스트 RPG용 LLM 하위 모듈입니다. 현재 범위는 게임 엔진 전체가 아니라, 플레이어 입력과 현재 상태를 받아 LLM 프롬프트를 만들고 검증된 `GameEvent`를 반환하는 부분입니다.

## 구성

- `src/llm/ILLMClient.hpp`: LLM 호출 인터페이스
- `src/llm/OllamaClient.*`: `ollama.hpp` 기반 Ollama chat 호출 클라이언트
- `src/llm/ContextBuilder.*`: `GameState`를 LLM context 문자열로 요약
- `src/llm/PromptBuilder.*`: 중세 판타지 GM 프롬프트 생성
- `src/llm/LLMOutputParser.*`: raw response에서 JSON 추출 및 `GameEvent` 변환
- `src/llm/LLMEventValidator.*`: 이벤트 타입, 선택지, 몬스터/아이템/보상 범위 검증 및 repair
- `src/llm/LLMFallbackFactory.*`: 실패 시 안전한 story 이벤트 생성
- `src/llm/LLMService.*`: GameEngine이 호출할 통합 진입점
- `tests/test_llm_module.cpp`: Parser, Validator, Fallback, Service 테스트

## 기본 사용

```cpp
#include "llm/LLMService.hpp"
#include "llm/OllamaClient.hpp"

using namespace textrpg::llm;

auto client = std::make_shared<OllamaClient>();
LLMService llmService(client);

GameState state;
state.turnNumber = 1;
state.world.location = "안개 숲 입구";

GameEvent event = llmService.generateEvent(state, "숲 안쪽으로 조심스럽게 들어간다");
```

## 실행 테스트

빌드 후 `llm_text_rpg`로 실제 Ollama 호출을 확인할 수 있습니다.

대화형 게임 루프:

```powershell
.\build\Debug\llm_text_rpg.exe
```

모델을 임시로 바꿔 실행:

```powershell
.\build\Debug\llm_text_rpg.exe llama3.2:latest
```

한 턴만 실행하고 종료하는 스크립트 입력:

```powershell
.\build\Debug\llm_text_rpg.exe llama3.2:latest "검을 뽑고 수풀 속에서 다가오는 적과 맞선다"
```

여러 턴을 자동 실행하려면 `|`로 행동을 구분합니다.

```powershell
.\build\Debug\llm_text_rpg.exe llama3.2:latest "늑대와 맞서 싸운다|쓰러진 늑대 주변의 흔적을 살핀다"
```

첫 번째 인자를 생략하면 `config/llm.toml`의 `[llm].model` 값을 사용합니다.
큰 로컬 모델은 첫 응답이 오래 걸릴 수 있으니 `config/llm.toml`의 `read_timeout_seconds` 값을 넉넉하게 둡니다.
생각 모드가 있는 Ollama 모델은 `think = false`로 `/set nothink`와 같은 요청을 보냅니다.
설정 로더는 같은 의미로 `nothink = true`도 지원합니다.

전투 이벤트가 생성되면 `event_type: combat`과 함께 `monster` 객체가 출력됩니다.
전투 이벤트는 선택지를 요구하지 않습니다. LLM은 몬스터와 전투 진입 상황만 만들고, 전투 행동은 별도 전투 시스템이 처리합니다.
각 LLM 이벤트는 `next_objective`와 `decision_hint`도 함께 반환합니다. 메인 루프는 이 값을 다음 턴의 목표와 판단 기준으로 반영하므로, 전투나 발견 이후에도 다음 장면이 이전 결과를 이어받습니다.

현재 `main.cpp`의 게임 루프는 데모용입니다. 플레이어 입력을 반복해서 받고, `GameEvent`를 출력하고, HP/Gold/Exp/최근 사건/인벤토리 후보 정도만 최소 반영합니다. 전투 계산, 선택지별 규칙 처리, 세이브/로드는 아직 별도 엔진 범위입니다.

## 빌드

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

외부 C++ 헤더는 모두 `third_party/`에 포함되어 있습니다.

- `third_party/ollama/ollama.hpp`
- `third_party/nlohmann/json.hpp`
- `third_party/spdlog/`
- `third_party/doctest/doctest.h`

CMake 빌드 중에 외부 라이브러리를 다운로드하지 않습니다. 헤더를 갱신하려면 해당 `third_party` 파일 또는 폴더를 새 버전으로 교체하면 됩니다.
