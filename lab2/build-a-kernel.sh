#!/bin/bash

$num_of_kernels=$(($num_of_kernels + 0))

apt install -y libncurses-dev dwarves build-essential gcc bc bison flex libssl-dev libelf-dev zstd

echo -e "Чекаем директорию с ядром"

if [ ! -d 'linux-5.10-103' ]; then

  echo -e 'Папки нет\nСкачиваю архив\n'

  wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.10.103.tar.xz

  if [ ! -f 'linux-5.10.103.tar.xz' ]; then
    echo -e "Скачивание провалилось, чекай инет\n"
    exit
  fi

  echo 'Скачал архив'
  echo 'Распаковываю'

  tar -xf linux-5.10.103.tar.xz

  if [ ! -d 'linux-5.10.103' ]; then
    echo 'Чет произошло, чекай лог ошибок'
    exit
  fi

  echo 'Распаковано'
fi

echo 'Перехожу в директорию'
cd linux-5.10.103

echo 'Ищу конфиг'
if [ ! -f '.config' ]; then
  echo 'Нету. Делаю новый'
  cp -v /boot/config-$(uname -r) .config
  echo 'Надо подрехтовать. Нажми exit и yes'
  sleep 10

  make menuconfig

  echo 'Рехтую конфиг'
  sed -i 's/debian\/[^"]*//g' .config
fi

echo -n "Введи кол-во потоков:"
read num_of_kernels

echo 'Сходи за чаем, это на долго'
sleep 2

if [ ! [ make -j $num_of_kernels && make modules -j $num_of_kernels] ]; then
  echo "Чет пошло не так, чекай логи"
  exit
fi

echo -e '\n\n\n\n\n\nУстанавливаю новое ядро'

make modules_install && make install

sed -i /boot/grub/grub.cfg 's/timeout=0/timeout=5/g'
sed -i /boot/grub/grub.cfg 's/set\ timeout_style=hidden//g'
