;;; cauta-mode.el -- Cauta editing mode
;;
;; References:
;; - http://emacs-fu.blogspot.com.br/2010/04/creating-custom-modes-easy-way-with.html
;; - http://www.emacswiki.org/cgi-bin/wiki/GenericMode
;; - http://ergoemacs.org/emacs/elisp_syntax_coloring.html
;; - http://alexdantas.net/stuff/posts/category/programming/emacs-lisp/

;;; Code:

(require 'generic-x)

(define-generic-mode 'cauta-mode

  ;; Starting delimiter for comments
  '("//")

  ;; Builtins
  '(
    "alias"
    "assert"
    "case"
    "continue"
    "default"
    "do"
    "else"
    "false"
    "final"
    "for"
    "func"
    "global"
    "goto"
    "if"
    "import"
    "index"
    "inline"
    "return"
    "sizeof"
    "struct"
    "switch"
    "true"
    "union"
    "while"
    "package"
    "packed"
    "local"
    "method"
    "null"
    "->"
    )

  ;; More keywords (first ones take precedence)
  '(
	;; Strings (only type supported is '' - not "")
	("\'.*\'" . 'font-lock-string-face)

    ;; Special tokens
	;;(" ->" . 'font-lock-builtin-face)

	;; Operators
	("\\b=\\b" . 'font-lock-operator-face)
	("\\b+\\b" . 'font-lock-operator-face)
	("\\b-\\b" . 'font-lock-operator-face)
	("\\b*\\b" . 'font-lock-operator-face)

	;; Builtins
	("void" . 'font-lock-builtin-face)
	("bool" . 'font-lock-builtin-face)
	("byte" . 'font-lock-builtin-face)
	("char" . 'font-lock-builtin-face)
	("short" . 'font-lock-builtin-face)
	("int" . 'font-lock-builtin-face)
	("long" . 'font-lock-builtin-face)
	("uchar" . 'font-lock-builtin-face)
	("ushort" . 'font-lock-builtin-face)
	("uint" . 'font-lock-builtin-face)
	("ulong" . 'font-lock-builtin-face)

	;; Special negation operator
	("\\bnot\\b"  . 'font-lock-negation-char-face)

	;; Numbers are highlighted
	("\\b[0-9]+\\b" . 'font-lock-variable-name-face)

	;; Matching the comment boundaries (only full words)
	("\\bquiet\\b" . 'font-lock-comment-delimiter-face)
	("\\bloud\\b"  . 'font-lock-comment-delimiter-face)

	)

  ;; File extensions for this mode
  '("\\.ca$")

  ;; Other functions to call
  (list
   (lambda ()
	 ;; Forcing to insert spaces on this mode
	 (setq tab-width 4)
	 (setq indent-tabs-mode 't)

	 ))

  ;; Doc string for this mode
  "Major mode for editing Cauta source code"
  )


(provide 'cauta-mode)
