# LLM Text RPG

중세 판타지 텍스트 RPG 프로토타입입니다. LLM은 장면과 이벤트 후보만 만들고, 전투 수치는 C++ 코드가 처리합니다.

## 구조

- `main.cpp`: 데모 게임 루프
- `src/llm/LLM.*`: Ollama 호출, 프롬프트 생성, JSON 추출, 최소 검증/fallback
- `src/llm/LLMTypes.hpp`: 게임 상태와 LLM 이벤트 타입
- `src/combat/CombatSystem.*`: 단순 전투 처리
- `src/combat/CombatTypes.hpp`: 전투 타입

## 현재 규칙

- LLM 백엔드: Ollama
- 기본 모델: `LLMOptions` 기본값인 `0xIbra/supergemma4-26b-uncensored-gguf-v2:Q4_K_M`
- 시작 위치, 첫 장면, 첫 목표, 첫 판단 기준은 LLM이 생성
- 매 턴 다음 이벤트 유형은 LLM이 현재 목표, 판단 기준, 최근 사건, 직전 행동을 보고 선택
  - 가능한 이벤트: `story`, `combat`, `item_gain`, `stat_change`, `dialogue`, `quest_update`, `rest`, `game_end`
  - C++ 엔진은 LLM 응답을 파싱/검증하고 전투 수치와 보상을 제한
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

## 빌드

```powershell
cmake -S . -B build
cmake --build build
```

## 실행

```powershell
.\build\Debug\llm_text_rpg.exe
```

모델 지정:

```powershell
.\build\Debug\llm_text_rpg.exe llama3.2:latest
```

한 턴 스크립트 실행:

```powershell
.\build\Debug\llm_text_rpg.exe llama3.2:latest "숲길의 부서진 표식을 조사한다"
```
