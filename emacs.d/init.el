;;-*-Emacs-Lisp-*-
;;
;; .emacs.d/init.el
;;


;;;; General:

(message "[ Loading Emacs init file of %s ]" (user-login-name))

;; Load-path (where to look for extra emacs lisp files)
(add-to-list 'load-path "~/.emacs.d/lisp/")

;; Where to look for extra emacs themes
(add-to-list 'custom-theme-load-path "~/.emacs.d/themes/")


;;;; Extra-modes:
(require 'cc-mode)
(require 'make-mode)
(require 'magit)
(require 'rst)
(require 'color-theme)
(require 'smart-tabs-mode)
(require 'generic-x)
(require 'cauta-mode)


;;;; Appearence:

;; Minbuffer's messages color
(set-face-foreground 'minibuffer-prompt "orange")

;; Disable toolbar in terminal-mode
(tool-bar-mode -1)

;; Disable menu-bar (toggle with "M-m", see key-binding below)
;; (menu-bar-mode -99)
(if (not window-system) (menu-bar-mode -99))

;; Show time
(display-time)

;; Flash the screen on error; don't beep.
;;(setq-default visible-bell t)

;; Highlight the marked region.
(setq-default transient-mark-mode t)

;; Highlight text selection
(transient-mark-mode 1)

;; Highligh current line
(global-hl-line-mode 1)

;; Display line, column numbers and buffer-size
(setq-default line-number-mode 't)
(setq-default column-number-mode 't)
(setq-default size-indication-mode 't)

;; Show line-number on left-side
(global-linum-mode t)
(if (window-system)
    (setq linum-format "%4d")
  (setq linum-format "%3d "))

;; New buffers are text mode
(setq default-major-mode 'text-mode)

;; Text lines limit to 80 chars
(setq-default fill-column 80)

;; Turn on syntax hilighting
(global-font-lock-mode)

;; Delete seleted text when typing
(delete-selection-mode 1)

;; Show trailing whitespaces
(setq-default show-trailing-whitespace t)

;; Make whitespace-mode use just basic coloring (no space)
(setq whitespace-style
      (quote (tabs newline tab-mark newline-mark)))

;; Highlight matching parantheses when the point is on them
(show-paren-mode 1)
(setq show-paren-delay 0)

;; Cursor type
(setq-default cursor-type 'bar)

;; Show cursor's column position
(column-number-mode 1)

;; Control level of font-lock decoration
(setq font-lock-maximum-decoration 2)

;; Frame focus follows mouse
(setq mouse-autoselect-window t)

;; Default font
(set-default-font "Monospace 11")



;;;; Behaviour:

;; Truncate lines at 80
(add-hook 'text-mode-hook 'turn-on-auto-fill)
;;(add-hook 'c-mode-hook 'turn-on-auto-fill)
;;(add-hook 'python-mode-hook 'turn-on-auto-fill)

;; Skip the startup screens
(setq-default inhibit-startup-screen t)

;; Do not create backup files
(setq make-backup-files nil) ; stop creating those backup~ files
(setq auto-save-default nil) ; stop creating those #auto-save# files

;; Force newline at ent-of-file
(setq require-final-newline 't)

;; Remove trailing whitespaces
(add-hook 'before-save-hook 'delete-trailing-whitespace)
(add-hook 'before-save-hook 'whitespace-cleanup)


;; Enable smoth-scrolling
(require 'smooth-scroll)
(smooth-scroll-mode t)

;;(setq mouse-wheel-scroll-amount '(1 ((shift) . 1)))
;;(setq mouse-wheel-progressive-speed nil)
(setq mouse-wheel-follow-mouse 't)
(setq scroll-conservatively 10000)
(setq auto-window-vscroll nil)

;; Do not ask before saving buffers
(setq buffer-save-without-query 't)

;; Use y/n instead of yes/no
;;(defalias 'yes-or-no-p 'y-or-n-p)
(fset 'yes-or-no-p 'y-or-n-p)

;; Save recent files
(require 'recentf)
(recentf-mode 1)
(setq recentf-max-menu-items 40)

;; Turn on save place for reopening a file at the same place
(require 'saveplace)
(setq save-place-file
      (concat user-emacs-directory "saveplace.el") )
(setq-default save-place t)

;; Start with enlarged window size
;;(if (window-system)
;;    (set-frame-size (selected-frame) 90 40))

;; When on a tab, make the cursor the tab length
(setq-default x-stretch-cursor t)


;;;; Key-binding:

;; Improved TAB behaviour
(global-set-key (kbd "DEL") 'backward-delete-char)

;; Toggle menu-bar
(global-set-key (kbd "M-m") 'menu-bar-mode)

;; Save
(global-set-key (kbd "C-x s") 'save-buffer)

;; Save all (without prompting for yes-or-no)
(defun save-some-buffers-now ()
  (interactive)
  (save-some-buffers t t))
(global-set-key (kbd "M-s") 'save-some-buffers-now)

;; Save and quit
(global-set-key (kbd "C-x q") 'save-buffers-kill-emacs)

;; Goto line
(global-set-key (kbd "C-x g") 'goto-line)

;; Ctrl-o to open a (new) file
(global-set-key (kbd "C-o") 'find-file)

;; Ctrl-k to close current file (buffer)
(global-set-key (kbd "C-k") 'kill-this-buffer)

;; Mark all
(global-set-key (kbd "C-a") 'mark-whole-buffer)

;; Window switching
(windmove-default-keybindings 'control)  ;; C-[direction]
(global-set-key (kbd "C-x -") 'rotate-windows)

;; Buffer navigation
(global-set-key "\M-[M" 'scroll-down)      ; PgUp = scroll-down
(global-set-key "\M-[H\M-[2J" 'scroll-up)  ; PgDn = scroll-up

;; Search forward with Ctrl-f
(global-set-key [(control f)] 'isearch-forward)
(define-key isearch-mode-map [(control f)]
  (lookup-key isearch-mode-map "\C-s"))
(define-key minibuffer-local-isearch-map [(control f)]
  (lookup-key minibuffer-local-isearch-map "\C-s"))

;; Search backward with Alt-f
(global-set-key [(meta f)] 'isearch-backward)
(define-key isearch-mode-map [(meta f)]
  (lookup-key isearch-mode-map "\C-r"))
(define-key minibuffer-local-isearch-map [(meta f)]
  (lookup-key minibuffer-local-isearch-map "\C-r"))

;; Changing font size
(global-set-key [(C-kp-add)] 'text-scale-increase)
(global-set-key [(C-kp-subtract)] 'text-scale-decrease)

;; Use cua-mode
(cua-mode t)
(setq cua-auto-tabify-rectangles nil)
(transient-mark-mode 1)
(setq cua-keep-region-after-copy t)

;; Open recent files
(global-set-key "\C-x\ \C-r" 'recentf-open-files)

;; Easy navigation pane
;;(require 'nav)
;;(nav-disable-overeager-window-splitting)
;;(global-set-key (kbd "M-n") 'nav-toggle)

;; Use ibuffer for buffer-listing
(require 'ibuffer)
(global-set-key (kbd "C-x C-b") 'ibuffer)
(autoload 'ibuffer "ibuffer" "List buffers" t)

;; Cycle through buffers with arrow-keys (Ctrl-left/right)
(global-set-key (kbd "C-<left>")  'bs-cycle-previous)
(global-set-key (kbd "C-<right>") 'bs-cycle-next)


;;;; Mode-setup:

;; Indentation rules
(setq-default c-basic-offset 4)
(setq-default tab-width 4)

(setq indent-line-function 'insert-tab)
(setq-default indent-tabs-mode t)

(add-hook 'makefile-mode-hook 'indent-tabs-mode)

;; Improved TAB behaviour
(setq c-backspace-function 'backward-delete-char)

;; Use smart tabs
(smart-tabs-insinuate 'c)
(setq-default indent-tabs-mode nil)

;; CC
(setq c-default-style
      '((java-mode . "java")
        (awk-mode . "awk")
        (other . "k&r")))

(add-hook 'c-mode-common-hook
          (lambda () (setq indent-tabs-mode t)))

(add-hook 'c-mode-common-hook
          '(lambda () (c-toggle-auto-state 1)))

;; CSS
(setq css-indent-offset 2)

;; Makefile
(add-hook 'makefile-mode-hook
          (lambda ()
            (whitespace-mode)
            (setq indent-tabs-mode t)
            (setq tab-width 8)))

;; SCons
(setq auto-mode-alist
      (cons '("SConstruct" . python-mode) auto-mode-alist))
(setq auto-mode-alist
      (cons '("SConscript" . python-mode) auto-mode-alist))

;; Shell-mode
(add-hook 'sh-mode-hook
          (lambda ()
            (setq tab-width 8
                  sh-basic-offset 8
                  indent-tabs-mode t)))

;;;; Colors:

;; Prefered color theme + private customizations
(color-theme-initialize)
(load-theme 'wombat t)
(set-face-underline-p 'highlight nil)
;;(set-background-color "black")
(custom-set-faces
 '(linum ((t (:foreground "gray50" :background "gray10" :box nil)))))
