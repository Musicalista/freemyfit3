@echo off
:: init_repo.bat — inicializa git local e prepara push para o GitHub
:: Execute este script na pasta C:\Users\Usuario\j2me-fit3

setlocal

echo ========================================
echo  Inicializando repositorio git local...
echo ========================================

cd /d "%~dp0"

if not exist ".git" (
    git init
    echo Repositorio git criado.
) else (
    echo Repositorio git ja existe.
)

git add .
git commit -m "feat: Fit3 toolkit — J2ME manager, Galaxy Watch theme, firmware pack" 2>nul || echo "Nada para commitar (ja commitado)."

echo.
echo ========================================
echo  PROXIMO PASSO (faça manualmente):
echo ========================================
echo 1. Crie um repositorio VAZIO em: https://github.com/new
echo 2. Copie a URL do repositorio (ex: https://github.com/seu-usuario/fit3-toolkit.git)
echo 3. Rode os comandos abaixo no terminal:
echo.
echo     git remote add origin https://github.com/seu-usuario/fit3-toolkit.git
echo     git branch -M main
echo     git push -u origin main
echo.
echo Atencao: nao versionamos o firmware_fit3_tudo_em_um.zip (esta no .gitignore).
echo ========================================

endlocal
pause
