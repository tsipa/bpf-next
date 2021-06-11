# $1 - start or end
# $2 - fold identifier, no spaces
# $3 - fold section description
travis_fold() {
	local YELLOW='\033[1;33m'
	local NOCOLOR='\033[0m'
	echo travis_fold:$1:$2
  if [ $1 = "start" ]; then
    line="::group::$2"
    if [ ! -z "${3:-}" ]; then
      line="$line - ${YELLOW}$3${NOCOLOR}"
    fi
  else
    line="::endgroup"
  fi
	echo -e "$line"
}
