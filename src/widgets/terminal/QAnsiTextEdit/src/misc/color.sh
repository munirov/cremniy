#!/bin/bash
#
#    This is slightly altered from the original which is found here:
#    http://www.tldp.org/HOWTO/Bash-Prompt-HOWTO/x329.html
#
#    Each line is the color code of one foreground color, out of 17
#    (default + 16 escapes), followed by a test use of that color
#    on all nine background colors (default + 8 escapes).
#

text="xYz"; # Some test text

echo -e "\n                40m   41m   42m   43m   44m   45m   46m   47m";

for FGs in '    m' '   1m' '  30m' '1;30m' '  31m' '1;31m' '  32m' \
           '1;32m' '  33m' '1;33m' '  34m' '1;34m' '  35m' '1;35m' \
           '  36m' '1;36m' '  37m' '1;37m'; do
    FG=${FGs// /}
    echo -en " $FGs \033[$FG  ${text}  ";
    for BG in 40m 41m 42m 43m 44m 45m 46m 47m; do
        echo -en "$EINS \033[$FG\033[${BG} ${text} \033[0m";
    done
    echo;
done
echo;

echo -e "\n\033[4;31mLight Colors\033[0m \t\t\t  \033[1;4;31mDark Colors\033[0m"
echo -e " \e[0;30;47m Black     \e[0m   0;30m \t\t \e[1;30;40m Dark Gray   \e[0m  1;30m"
echo -e " \e[0;31;47m Red       \e[0m   0;31m \t\t \e[1;31;40m Dark Red    \e[0m  1;31m"
echo -e " \e[0;32;47m Green     \e[0m   0;32m \t\t \e[1;32;40m Dark Green  \e[0m  1;32m"
echo -e " \e[0;33;47m Brown     \e[0m   0;33m \t\t \e[1;33;40m Yellow      \e[0m  1;33m"
echo -e " \e[0;34;47m Blue      \e[0m   0;34m \t\t \e[1;34;40m Dark Blue   \e[0m  1;34m"
echo -e " \e[0;35;47m Magenta   \e[0m   0;35m \t\t \e[1;35;40m Dark Magenta\e[0m  1;35m"
echo -e " \e[0;36;47m Cyan      \e[0m   0;36m \t\t \e[1;36;40m Dark Cyan   \e[0m  1;36m"
echo -e " \e[0;37;47m Light Gray\e[0m   0;37m \t\t \e[1;37;40m White       \e[0m  1;37m"

