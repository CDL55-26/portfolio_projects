# Dockerfile
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential make git python3 ca-certificates \
    qemu-system-misc qemu-system-riscv32 qemu-system-riscv64 \
    gdb-multiarch \
    gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
    openssh-client \
 && rm -rf /var/lib/apt/lists/*

# Nice to have: less, vim, etc. (optional)
RUN apt-get update && apt-get install -y --no-install-recommends \
      vim-nox universal-ctags curl \
  && rm -rf /var/lib/apt/lists/* \
  # install vim-plug for root
  && curl -fLo /root/.vim/autoload/plug.vim --create-dirs \
       https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim \
  # minimal .vimrc: ALE for gcc diagnostics, tags for completion/jumps
  && printf '%s\n' \
"call plug#begin(\"/root/.vim/plugged\")" \
"Plug 'dense-analysis/ale'" \
"call plug#end()" \
"set number | syntax on" \
"set tabstop=4 shiftwidth=4 expandtab" \
"set updatetime=300 | set signcolumn=yes" \
"set complete=.,w,b,u,t,i | set wildmenu | set tags=./tags,tags" \
"let g:ale_linters = {'c': ['gcc']}" \
"let g:ale_c_gcc_options = '-std=c11 -Wall -Wextra -Wpedantic -O0 -Iinclude'" \
"let g:ale_set_signs = 1 | let g:ale_sign_error = \"✘\" | let g:ale_sign_warning = \"▲\"" \
"nnoremap <leader>n :ALENext<CR> | nnoremap <leader>p :ALEPrevious<CR>" \
> /root/.vimrc \
  # pre-install the plugin to avoid doing it interactively
  && vim +PlugInstall +qall
WORKDIR /work


