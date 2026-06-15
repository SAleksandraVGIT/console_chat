#!/usr/bin/env bash

# Останавливает скрипт при ошибке команды, обращении к неопределённой переменной
# или ошибке любой команды внутри конвейера.
set -euo pipefail

# Определяет абсолютные пути к каталогу скрипта и корню проекта.
# Благодаря этому скрипт можно запускать из любого рабочего каталога.
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"

# Значения подключения и имена создаваемых объектов по умолчанию.
# Любое из них можно изменить соответствующим аргументом командной строки.
database="console_chat"
app_user="console_chat"
app_host="127.0.0.1"
port="3306"
root_user="root"
config_path="${project_root}/config/mysql.conf"
use_sudo=false

# Выводит справку по доступным аргументам скрипта.
usage() {
    cat <<'EOF'
Использование: scripts/setup_mysql.sh [параметры]

Параметры:
  --database <имя>       Имя базы данных (по умолчанию: console_chat)
  --user <имя>           Пользователь приложения (по умолчанию: console_chat)
  --host <адрес>         Адрес MySQL и host пользователя (по умолчанию: 127.0.0.1)
  --port <номер>         Порт MySQL (по умолчанию: 3306)
  --root-user <имя>      Администратор MySQL (по умолчанию: root)
  --config <путь>        Путь к создаваемому конфигу (по умолчанию: config/mysql.conf)
  --sudo                 Войти в локальный MySQL через sudo и Unix-сокет
  --help                 Показать эту справку

Пароль приложения читается из MYSQL_APP_PASSWORD или запрашивается скрыто.
Без --sudo пароль администратора отдельно запрашивает клиент mysql.
С --sudo может быть запрошен пароль системного пользователя Linux.
EOF
}

# Проверяет, что после аргумента, например --database, передано значение.
require_value() {
    if [[ $# -lt 2 ]]; then
        printf 'Не указано значение для %s\n' "$1" >&2
        exit 1
    fi

    if [[ -z "$2" ]]; then
        printf 'Не указано значение для %s\n' "$1" >&2
        exit 1
    fi
}

# Разбирает аргументы и заменяет ими значения по умолчанию.
while [[ $# -gt 0 ]]; do
    case "$1" in
        --database)
            require_value "$@"
            database="$2"
            shift 2
            ;;
        --user)
            require_value "$@"
            app_user="$2"
            shift 2
            ;;
        --host)
            require_value "$@"
            app_host="$2"
            shift 2
            ;;
        --port)
            require_value "$@"
            port="$2"
            shift 2
            ;;
        --root-user)
            require_value "$@"
            root_user="$2"
            shift 2
            ;;
        --config)
            require_value "$@"
            config_path="$2"
            shift 2
            ;;
        --sudo)
            use_sudo=true
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            printf 'Неизвестный параметр: %s\n' "$1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

# Проверяет имя базы перед подстановкой в SQL-запрос.
if [[ ! "$database" =~ ^[A-Za-z][A-Za-z0-9_]*$ ]]; then
    printf 'Имя базы может содержать только буквы, цифры и знак подчёркивания.\n' >&2
    exit 1
fi

# Проверяет имя технического пользователя MySQL.
if [[ ! "$app_user" =~ ^[A-Za-z][A-Za-z0-9_]*$ ]]; then
    printf 'Имя пользователя может содержать только буквы, цифры и знак подчёркивания.\n' >&2
    exit 1
fi

# Разрешает в адресе сервера только символы, используемые в hostname и IP-адресах.
if [[ ! "$app_host" =~ ^[A-Za-z0-9._%:-]+$ ]]; then
    printf 'Адрес сервера содержит недопустимые символы.\n' >&2
    exit 1
fi

# Проверяет, что порт является числом из допустимого диапазона TCP/UDP.
if [[ ! "$port" =~ ^[0-9]+$ ]] || ((port < 1 || port > 65535)); then
    printf 'Порт должен находиться в диапазоне 1..65535.\n' >&2
    exit 1
fi

# Получает пароль технического пользователя приложения.
# Для автоматического запуска используется MYSQL_APP_PASSWORD, иначе пароль
# запрашивается дважды и не отображается в терминале благодаря флагу read -s.
if [[ -n "${MYSQL_APP_PASSWORD+x}" ]]; then
    app_password="$MYSQL_APP_PASSWORD"
else
    read -r -s -p "Пароль для пользователя MySQL '${app_user}': " app_password
    printf '\n'
    read -r -s -p "Повторите пароль: " repeated_password
    printf '\n'

    if [[ "$app_password" != "$repeated_password" ]]; then
        printf 'Пароли не совпадают.\n' >&2
        exit 1
    fi
fi

# Запрещает пустой пароль технического пользователя.
if [[ -z "$app_password" ]]; then
    printf 'Пароль приложения не должен быть пустым.\n' >&2
    exit 1
fi

# Не допускает переносы строк и пробелы по краям, поскольку конфигурация
# записывается в текстовом формате key=value.
if [[ "$app_password" == *$'\n'* || "$app_password" == *$'\r'* ||
      "$app_password" =~ ^[[:space:]] || "$app_password" =~ [[:space:]]$ ]]; then
    printf 'Пароль не должен содержать переносы строк или пробелы по краям.\n' >&2
    exit 1
fi

# Экранирует обратные слеши и одинарные кавычки для строкового литерала SQL.
sql_password=${app_password//\\/\\\\}
sql_password=${sql_password//\'/\'\'}

# Выбирает способ административного подключения к MySQL.
# В обычном режиме mysql запросит пароль MySQL-пользователя root_user.
# В режиме --sudo используется локальный Unix-сокет и системная авторизация Ubuntu;
# sudo при необходимости запросит пароль текущего пользователя Linux.
if [[ "$use_sudo" == true ]]; then
    mysql_command=(sudo mysql --user="$root_user")
else
    mysql_command=(
        mysql
        --host="$app_host"
        --port="$port"
        --user="$root_user"
        --password
    )
fi

# Формирует SQL в стандартном потоке и передаёт его выбранной команде mysql.
# Запросы создают базу и пользователя, обновляют его пароль, выдают права,
# выбирают созданную базу и добавляют таблицы из database/schema.sql.
printf 'Создание базы, технического пользователя и таблиц...\n'
{
    printf "SET SESSION sql_mode = REPLACE(@@SESSION.sql_mode, 'NO_BACKSLASH_ESCAPES', '');\n"
    printf 'CREATE DATABASE IF NOT EXISTS `%s` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n' "$database"
    printf "CREATE USER IF NOT EXISTS '%s'@'%s' IDENTIFIED BY '%s';\n" "$app_user" "$app_host" "$sql_password"
    printf "ALTER USER '%s'@'%s' IDENTIFIED BY '%s';\n" "$app_user" "$app_host" "$sql_password"
    printf 'GRANT SELECT, INSERT, UPDATE, DELETE ON `%s`.* TO '\''%s'\''@'\''%s'\'';\n' "$database" "$app_user" "$app_host"
    printf 'USE `%s`;\n' "$database"
    cat "${project_root}/database/schema.sql"
} | "${mysql_command[@]}"

# Создаёт локальный конфигурационный файл для будущей загрузки MySQLManager.
# umask 077 и chmod 600 разрешают читать пароль только владельцу файла.
mkdir -p "$(dirname -- "$config_path")"
umask 077
{
    printf 'host=%s\n' "$app_host"
    printf 'port=%s\n' "$port"
    printf 'user=%s\n' "$app_user"
    printf 'password=%s\n' "$app_password"
    printf 'database=%s\n' "$database"
    printf 'connect_timeout_seconds=5\n'
    printf 'charset=utf8mb4\n'
} > "$config_path"
chmod 600 "$config_path"

# Сообщает путь к готовой локальной конфигурации.
printf 'MySQL настроен. Локальная конфигурация: %s\n' "$config_path"
