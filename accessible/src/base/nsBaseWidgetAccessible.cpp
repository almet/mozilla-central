/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
 *   Alexander Surkov <surkov.alexander@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsBaseWidgetAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsHyperTextAccessibleWrap.h"
#include "Role.h"
#include "States.h"

#include "nsGUIEvent.h"
#include "nsILink.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsLeafAccessible
////////////////////////////////////////////////////////////////////////////////

nsLeafAccessible::
  nsLeafAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsLeafAccessible, nsAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsLeafAccessible: nsAccessible public

nsAccessible*
nsLeafAccessible::ChildAtPoint(PRInt32 aX, PRInt32 aY,
                               EWhichChildAtPoint aWhichChild)
{
  // Don't walk into leaf accessibles.
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// nsLeafAccessible: nsAccessible private

void
nsLeafAccessible::CacheChildren()
{
  // No children for leaf accessible.
}


////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible
////////////////////////////////////////////////////////////////////////////////

nsLinkableAccessible::
  nsLinkableAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsAccessibleWrap(aContent, aDoc),
  mActionAcc(nsnull),
  mIsLink(false),
  mIsOnclick(false)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsLinkableAccessible, nsAccessibleWrap)

////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible. nsIAccessible

NS_IMETHODIMP
nsLinkableAccessible::TakeFocus()
{
  return mActionAcc ? mActionAcc->TakeFocus() : nsAccessibleWrap::TakeFocus();
}

PRUint64
nsLinkableAccessible::NativeState()
{
  PRUint64 states = nsAccessibleWrap::NativeState();
  if (mIsLink) {
    states |= states::LINKED;
    if (mActionAcc->State() & states::TRAVERSED)
      states |= states::TRAVERSED;
  }

  return states;
}

void
nsLinkableAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  nsAccessible::Value(aValue);
  if (!aValue.IsEmpty())
    return;

  if (aValue.IsEmpty() && mIsLink)
    mActionAcc->Value(aValue);
}


PRUint8
nsLinkableAccessible::ActionCount()
{
  return (mIsOnclick || mIsLink) ? 1 : 0;
}

NS_IMETHODIMP
nsLinkableAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  // Action 0 (default action): Jump to link
  if (aIndex == eAction_Jump) {   
    if (mIsLink) {
      aName.AssignLiteral("jump");
      return NS_OK;
    }
    else if (mIsOnclick) {
      aName.AssignLiteral("click");
      return NS_OK;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsLinkableAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Jump)
    return NS_ERROR_INVALID_ARG;

  return mActionAcc ? mActionAcc->DoAction(aIndex) :
    nsAccessibleWrap::DoAction(aIndex);
}

KeyBinding
nsLinkableAccessible::AccessKey() const
{
  return mActionAcc ?
    mActionAcc->AccessKey() : nsAccessible::AccessKey();
}

////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible. nsAccessNode

void
nsLinkableAccessible::Shutdown()
{
  mIsLink = false;
  mIsOnclick = false;
  mActionAcc = nsnull;
  nsAccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible: HyperLinkAccessible

already_AddRefed<nsIURI>
nsLinkableAccessible::AnchorURIAt(PRUint32 aAnchorIndex)
{
  if (mIsLink) {
    NS_ASSERTION(mActionAcc->IsLink(),
                 "nsIAccessibleHyperLink isn't implemented.");

    if (mActionAcc->IsLink())
      return mActionAcc->AnchorURIAt(aAnchorIndex);
  }

  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible: nsAccessible protected

void
nsLinkableAccessible::BindToParent(nsAccessible* aParent,
                                   PRUint32 aIndexInParent)
{
  nsAccessibleWrap::BindToParent(aParent, aIndexInParent);

  // Cache action content.
  mActionAcc = nsnull;
  mIsLink = false;
  mIsOnclick = false;

  if (nsCoreUtils::HasClickListener(mContent)) {
    mIsOnclick = true;
    return;
  }

  // XXX: The logic looks broken since the click listener may be registered
  // on non accessible node in parent chain but this node is skipped when tree
  // is traversed.
  nsAccessible* walkUpAcc = this;
  while ((walkUpAcc = walkUpAcc->Parent()) && !walkUpAcc->IsDoc()) {
    if (walkUpAcc->Role() == roles::LINK &&
        walkUpAcc->State() & states::LINKED) {
        mIsLink = true;
        mActionAcc = walkUpAcc;
        return;
    }

    if (nsCoreUtils::HasClickListener(walkUpAcc->GetContent())) {
      mActionAcc = walkUpAcc;
      mIsOnclick = true;
      return;
    }
  }
}

void
nsLinkableAccessible::UnbindFromParent()
{
  mActionAcc = nsnull;
  mIsLink = false;
  mIsOnclick = false;

  nsAccessibleWrap::UnbindFromParent();
}

////////////////////////////////////////////////////////////////////////////////
// nsEnumRoleAccessible
////////////////////////////////////////////////////////////////////////////////

nsEnumRoleAccessible::
  nsEnumRoleAccessible(nsIContent* aNode, nsDocAccessible* aDoc,
                       roles::Role aRole) :
  nsAccessibleWrap(aNode, aDoc), mRole(aRole)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsEnumRoleAccessible, nsAccessible)

role
nsEnumRoleAccessible::NativeRole()
{
  return mRole;
}
