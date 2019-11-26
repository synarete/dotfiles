# Bash-completion helpers

# Require bash
[ -z "$BASH_VERSION" ] && return


__cauta_tool_complete() {
  COMPREPLY=()
  local IFS=$' \n'
  local cur="${COMP_WORDS[COMP_CWORD]}"
  local opts=${1:-}
  local cmds=${2:-}

  if [[ ${cur} == -* ]]; then
    COMPREPLY=( ${COMPREPLY[@]:-} \
      $(compgen -S ' ' -W "${opts}" -- "${cur}" ) )
  fi
  if [[ ${COMP_CWORD} == 1 ]]; then
    COMPREPLY=( ${COMPREPLY[@]:-} \
    $(compgen -S ' ' -W "${cmds}" -- "${cur}") )
  fi
}


__cautac() {
  local opts="-v --version -h --help "
  opts+="-m --main -o --output"
  __cauta_tool_complete "${opts}"
}


# Completion hooks:
complete -o bashdefault -o default -o nospace -F __cautac cautac

