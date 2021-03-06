# Solution
Основой процессора является ассоциативный контейнер каналов `channels_`,
каждый элемент которого (`class Channel`) содержит очередь данных `Channel::queue_` конкретного канала, назначенный обработчик этих данных `Channel::consumer_` и мьютекс `Channel::mutex_` для синхонизации доступа к данным канала.

Основная логика работы процессора сосредоточена в методе `do_operation_in_channel_`, который сначала выполняет поиск требуемого канала, а при его отстутствии создает новый канал, и далее выполняет действие в этом канале, переданное, как аргумент метода `Func func`.
Всего таких действий три:
- назначение обработчика `Subscribe`
- удаление обработчика `Unsubscribe`
- вставка данных в очередь `Enqueue`

Создание нового канала при его отсуствии требуется не всегда. Например, при *(ошибочной)* попытке удаления обработчика несуществующего канала, нет необходимости создавать этот канал. За включение/отключение логики создания нового канала в методе `do_operation_in_channel_` отвечает шаблонный параметр `Create`.

Логично предположить, что основная работа будет происходить на существующих каналах, а добавление новых каналов будет более редким явлением.
Синхронизация доступа к контейнеру `channels_` требуется только при добавлении нового канала. Следовательно, на этом уровне целесообразно использовать `shared_mutex`. Это позволяет блокировать контейнер `channels_` только для добавления новых каналов, а в остальное время блокировка происходит только в конкретных каналов за счет их внутреннего мьютекса `Channel::mutex_`.

При достижении лимита очереди конкретного канала возможны две стратегии :
- вытесняющая элемент из головы очереди;
- отвергающая вставку новых элементов с возвращением `false` из метода.

За это отвечет шаблонный параметр класса `EjectingStrategy`.
Само же значение лимита передается в конструктор класса, т.к. лимит может определяться в рантайме.

Для снижения нагрузки на систему, создаваемой циклом метода `Process`, задействован `semaphor_`.
Но целесообразность его использования под сомнением, т.к. он работает только при наличии обработчика в каждом канале. В противном случае, такой семафор не снижает нагрузку. Поэтому вместе с семафором был оставлен и `std::this_thread::yield()`.
В принципе, включение/отключение использования семафора можно так же вынести в настройки класса (*traits*).

Так же для оптимизации быстродействия в методе `Cannel::consume` производится предварительная (до захвата мьютекса) проверка готовности канала на наличие данных и обработчика.

Тестовый пример выполнен в виде unittest'ов на базе **GoogleTest**.

При выполнении тестов эмулируется вычислительная нагрузка в обработчиках `Consumer::Consume` с помощью `sleep_for(20ms)`. Поэтому тесты выполняются относительно долго.

## Requirements
* C++ compiler (defaults to gcc)
* [CMake](https://cmake.org/)
* [GoogleTest](https://github.com/google/googletest)
```
$ sudo apt-get install libgtest-dev
```
