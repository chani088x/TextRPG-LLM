# LLM Text RPG

중세 판타지 텍스트 RPG 프로토타입입니다. LLM은 장면과 이벤트 후보만 만들고, 전투 수치는 C++ 코드가 처리합니다.

## 구조

- `main.cpp`: 데모 게임 루프
- `src/llm/LLM.*`: Ollama 호출, 프롬프트 생성, JSON 추출, 최소 검증/fallback
- `src/llm/LLMTypes.hpp`: 게임 상태와 LLM 이벤트 타입
- `src/combat/CombatSystem.*`: 단순 전투 처리
- `src/combat/CombatTypes.hpp`: 전투 타입
- `tests/test_llm_module.cpp`: LLM JSON 파싱과 전투 테스트

## 현재 규칙

- LLM 백엔드: Ollama
- 기본 모델: `llama3.2:latest`
- 플레이어: HP 999, 공격력 9
- 몬스터: HP 10, 공격력 1
- 전투 스킬: 양쪽 모두 `때리기`만 사용
- LLM이 준 몬스터 수치는 무시하고 이름/설명만 사용

## 빌드

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
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
