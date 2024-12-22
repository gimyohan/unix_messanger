# UNIX 프로그래밍 설계 과제 #3

## 과제 설명

N명의 사용자가 서로 문자를 주고받으며, 대화를 할 수 있는 `talk` 프로그램을 `semaphore`와 `shared memory segment`를 이용하여 작성합니다.

### 상세 요구 사항

#### (a)
- 통신 매체로는 **버퍼 크기가 10**인 1개의 `shared memory segment`를 사용하고, 동기화는 `semaphore`를 이용하여 구현합니다.
- 통신 메시지를 저장하는 크기 10의 `shared memory segment` 외에 추가 정보를 저장할 다른 `shared memory segment`를 사용할 수도 있습니다.

#### (b)
- 통신에 참여하는 인원은 고정되어 있지는 않지만, **동시에 통신할 수 있는 인원은 최대 N명**입니다.

#### (c)
- 통신을 시작하면, 다음 메시지가 화면에 출력됩니다.
  ```
  id=N1, talkers=N2, msg#=N3
  ```
  - 자신을 제외한 통신 참여 인원이 0명일 때 메시지를 입력하면, 위와 동일한 메시지가 출력됩니다.
  - 단, 참여 인원이 0명이라도 메시지를 입력하면 `msg#`는 1씩 증가합니다.
  - `msg#`는 0에서 시작해서 9까지 증가한 후, 다시 0이 됩니다.

#### (d)
- 통신을 시작할 때, 통신 ID를 `main` 함수의 인자로 전달하며, `id`는 1부터 N 사이의 정수값을 ID로 사용할 수 있습니다.
- 모든 메시지는 보낸 사용자의 ID와 함께 출력됩니다.
  ```
  [sender=N1 & msg#=N2] 메시지 내용
  ```

#### (e)
- 각 사용자의 문자열 입력과 다른 사용자로부터 전달된 문자열의 출력 작업은 **비동기적으로** 이루어집니다.

#### (f)
- 동일한 문자열이 두 번 이상 출력되거나, 사용자 중 한 명이라도 문자열 누락이 발생하지 않도록 해야 합니다.
- 어떤 경우에도 사용자가 문자를 입력한 출력 순서가 달라지는 것은 허용되지 않습니다.

#### (g)
- 버퍼의 크기는 10이며, 통신에 참여한 인원 중 한 명 이상의 데이터 읽기 속도가 매우 느려 버퍼가 가득 찬 경우, 버퍼에 대한 쓰기 작업은 **잠시 blocking**될 수도 있습니다.

#### (h)
- 자신이 입력한 문자열을 자신이 다시 출력하지는 않습니다.

#### (i)
- 사용자가 `talk_quit` 메시지를 보내면, 해당 사용자의 `talk` 프로그램은 종료됩니다. `talk_quit` 메시지는 ID와 함께 다른 사용자들에게 전달됩니다.

#### (j)
- 마지막으로 `talk_quit`을 입력하는 사용자는 사용했던 `shared memory segment`와 `semaphore`를 삭제하고 종료됩니다.

---
